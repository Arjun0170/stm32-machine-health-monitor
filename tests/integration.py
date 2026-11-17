#!/usr/bin/env python3
"""Drive real ARM ELFs through Renode's Monitor; assertions consume receiver UART."""

import argparse
import fcntl
import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RENODE = (
    os.environ.get("RENODE")
    or shutil.which("renode")
    or next(
        (
            str(p)
            for p in [
                ROOT / ".tools/renode_1.17.0-portable/renode",
                Path.home() / "tools/renode_1.17.0-portable/renode",
            ]
            if p.is_file()
        ),
        "renode",
    )
)


class Emulator:
    def __init__(self):
        self.log = (ROOT / "build/renode.log").open("w")
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]
        self.process = subprocess.Popen(
            [RENODE, "--disable-gui", "--plain", "--port", str(port)],
            cwd=ROOT,
            stdout=self.log,
            stderr=subprocess.STDOUT,
        )
        deadline = time.monotonic() + 30
        while True:
            try:
                self.socket = socket.create_connection(("127.0.0.1", port), timeout=1)
                break
            except OSError:
                if self.process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError((ROOT / "build/renode.log").read_text())
                time.sleep(0.1)
        self.socket.settimeout(30)
        self.read_prompt()

    def read_prompt(self):
        data = b""
        while not re.search(rb"\([^\r\n]*\)\s*$", data):
            part = self.socket.recv(65536)
            if not part:
                raise RuntimeError("Monitor disconnected")
            data += part
        return data.decode(errors="replace")

    def cmd(self, command):
        self.socket.sendall(command.encode() + b"\n")
        result = self.read_prompt()
        if re.search(r"(Error|Exception|Could not|No such|Failed|error CS\d)", result):
            raise RuntimeError(command + "\n" + result)
        return result

    def run(self, seconds):
        self.cmd(f'emulation RunFor "{seconds:.6f}"')

    def close(self):
        try:
            self.socket.sendall(b"quit\n")
        except OSError:
            pass
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            self.process.wait(timeout=5)
        self.socket.close()
        self.log.close()


def setup(e, probe=False):
    e.cmd("include @" + str(ROOT / "renode/Sensors.cs"))
    e.cmd('emulation CreateCANHub "canHub"')
    e.cmd('emulation SetGlobalQuantum "0.000025"')
    e.cmd("emulation SetGlobalSerialExecution True")
    for name, elf in [
        ("receiver", "receiver"),
        ("monitor", "probe" if probe else "monitor"),
    ]:
        e.cmd('mach create "' + name + '"')
        e.cmd("machine LoadPlatformDescription @" + str(ROOT / "renode/h743.repl"))
        e.cmd("connector Connect sysbus.fdcan1 canHub")
        e.cmd("sysbus LoadELF @" + str(ROOT / f"build/{elf}.elf"))
        path = ROOT / f"build/{name}.uart.log"
        if path.exists():
            path.unlink()
        e.cmd("sysbus.usart3 CreateFileBackend @" + str(path))


def uart_text(name):
    return (ROOT / f"build/{name}.uart.log").read_text(errors="replace")


def uart_write(e, line):
    # Input enters the emulated USART RX FIFO, never firmware RAM.
    for char in line + "\n":
        e.cmd("sysbus.usart3 WriteChar " + str(ord(char)))


def decoded_frames():
    pattern = re.compile(
        r"RX t=(\d+) id=(\d+) dlc=(\d+) format=(\d+) raw=([0-9a-f]{16}) temp=(-?\d+) vibration=(\d+) flags=(\d+) seq=(\d+)"
    )
    frames = []
    for match in pattern.finditer(uart_text("receiver")):
        t, ident, dlc, fmt, raw, temp, vib, flags, seq = match.groups()
        frames.append(
            {
                "t": int(t),
                "id": int(ident),
                "dlc": int(dlc),
                "format": int(fmt),
                "raw": raw,
                "temp": int(temp),
                "vibration": int(vib),
                "flags": int(flags),
                "seq": int(seq),
            }
        )
    return frames


def assert_received_payloads(frames):
    import struct

    assert len(frames) >= 12, f"Expected received telemetry, got {frames}"
    for i, f in enumerate(frames):
        assert (f["id"], f["dlc"], f["format"]) == (0x180, 8, 0), f
        expected = struct.pack(
            "<hHBBBB", f["temp"], f["vibration"], f["flags"], f["seq"], 0, 0
        ).hex()
        assert f["raw"] == expected, f
        assert f["flags"] & ~3 == 0, f
        assert f["seq"] == i % 256, f"Lost, duplicated, or out-of-order CAN frame: {f}"


def assert_stage(frames, stages, name, *, temp, vibration, flags):
    stage = next(s for s in stages if s["name"] == name)
    got = [
        f
        for f in frames
        if stage["start_ms"] <= f["t"] <= stage["end_ms"] + 2
        and (f["temp"], f["vibration"], f["flags"]) == (temp, vibration, flags)
    ]
    assert got, (
        f"{name}: missing receiver payload {(temp, vibration, flags)} in {stage}; {frames}"
    )
    return got


def full_sequence(e):
    stages = []
    elapsed = 0

    def stage(name, duration, *commands):
        nonlocal elapsed
        for cmd in commands:
            e.cmd(cmd)
        start = round(elapsed * 1000)
        e.run(duration)
        elapsed += duration
        stages.append(
            {"name": name, "start_ms": start, "end_ms": round(elapsed * 1000)}
        )

    stage("baseline", 0.62)
    uart_write(e, "threshold 400")
    stage("lower threshold", 0.04)
    stage(
        "alarm",
        0.20,
        "sysbus.spi4.accel X -900",
        "sysbus.spi4.accel Y 200",
        "sysbus.spi4.accel Z 32000",
    )
    stage(
        "at threshold clears",
        0.15,
        "sysbus.spi4.accel X 400",
        "sysbus.spi4.accel Y -400",
    )
    stage("alarm again", 0.15, "sysbus.spi4.accel X -900", "sysbus.spi4.accel Y 200")
    stage("temperature missing response", 1.1, "sysbus.i2c1.sensor Available false")
    uart_write(e, "status")
    uart_write(e, "threshold 1000")
    stage("console during fault", 0.06)
    uart_write(e, "threshold 400")
    stage("alarm during fault", 0.06)
    stage(
        "temperature restored",
        0.8,
        "sysbus.i2c1.sensor Temperature -123",
        "sysbus.i2c1.sensor Available true",
    )
    stage("alarm clears", 0.15, "sysbus.spi4.accel X 400", "sysbus.spi4.accel Y -400")
    stage("signed extrema", 0.15, "sysbus.spi4.accel X -32768", "sysbus.spi4.accel Y 0")
    for cmd in [
        "threshold 99",
        "threshold 4001",
        "threshold -100",
        "threshold 4294967296000",
        "threshold",
        "nonsense",
        "x" * 64,
    ]:
        uart_write(e, cmd)
    for cmd in ["threshold 100", "threshold 4000", "status"]:
        uart_write(e, cmd)
    stage("parser recovery", 0.12)
    counts = {}
    for key, command in [
        ("i2c", "sysbus.i2c1.sensor Reads"),
        ("spi", "sysbus.spi4.accel Reads"),
    ]:
        result = e.cmd(command)
        values = re.findall(r"0x([0-9a-fA-F]+)", result)
        assert values, result
        counts[key] = int(values[-1], 16)
    assert counts["i2c"] == 7 and counts["spi"] in (35, 36), counts
    return stages, counts


def assert_full_sequence(frames, stages):
    assert_received_payloads(frames)
    assert frames[0]["raw"] == "0000000000000000", frames[
        0
    ]  # Before first valid temperature.
    assert_stage(frames, stages, "baseline", temp=250, vibration=100, flags=0)
    alarm = assert_stage(frames, stages, "alarm", temp=250, vibration=900, flags=1)[0]
    clear = assert_stage(
        frames, stages, "at threshold clears", temp=250, vibration=400, flags=0
    )[0]
    fault = assert_stage(
        frames, stages, "temperature missing response", temp=250, vibration=900, flags=3
    )
    assert len(fault) >= 3, "Periodic CAN must continue during the I2C timeout"
    assert_stage(
        frames, stages, "console during fault", temp=250, vibration=900, flags=2
    )
    assert_stage(frames, stages, "alarm during fault", temp=250, vibration=900, flags=3)
    restore = assert_stage(
        frames, stages, "temperature restored", temp=-123, vibration=900, flags=1
    )[0]
    assert_stage(frames, stages, "alarm clears", temp=-123, vibration=400, flags=0)
    assert_stage(frames, stages, "signed extrema", temp=-123, vibration=32768, flags=1)
    for deadline in range(500, stages[-1]["end_ms"], 500):
        assert any(abs(f["t"] - deadline) <= 3 for f in frames), (
            f"Missing periodic CAN frame at {deadline} ms"
        )
    for name, f in [("alarm", alarm), ("at threshold clears", clear)]:
        start = next(s["start_ms"] for s in stages if s["name"] == name)
        assert f["t"] - start <= 103, f"Flag change frame delayed: {name}"
    console = uart_text("monitor")
    assert "OK threshold=400" in console and "OK threshold=1000" in console
    assert re.search(
        r"STATUS t=\d+ temp=250 vibration=900 threshold=400 flags=3", console
    ), console
    assert console.count("ERR invalid command or threshold") == 5, console
    assert (
        console.count("ERR unknown command") == 1
        and console.count("ERR command too long") == 1
    ), console
    assert "OK threshold=100\n" in console and "OK threshold=4000\n" in console
    assert re.search(
        r"STATUS t=\d+ temp=-123 vibration=32768 threshold=4000 flags=1", console
    ), console
    return {
        "received_frames": len(frames),
        "first_alarm_ms": alarm["t"],
        "alarm_clear_ms": clear["t"],
        "first_temperature_fault_ms": fault[0]["t"],
        "temperature_restored_ms": restore["t"],
        "duration_ms": stages[-1]["end_ms"],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", action="store_true")
    args = parser.parse_args()
    e = Emulator()
    try:
        setup(e, args.probe)
        if args.probe:
            e.run(0.1)
            uart_write(e, "s")
            e.run(0.02)
            e.close()
            e = None
            assert "PROBE I2C ok=1 temp=250 SPI ok=1 xyz=100 -50 1000" in uart_text(
                "monitor"
            )
            assert "PROBE UART RX OK" in uart_text("monitor")
            assert (
                "id=384 dlc=8 format=0 raw=fa00640000000000 temp=250 vibration=100 flags=0 seq=0"
                in uart_text("receiver")
            )
            evidence = ROOT / "evidence/probe.txt"
            evidence.write_text(
                uart_text("monitor")
                + uart_text("receiver")
                + "PASS H743 UART RX/TX, I2C, SPI, two-node Classical CAN\n"
            )
            print(evidence.read_text())
        else:
            stages, counts = full_sequence(e)
            e.close()
            e = None
            frames = decoded_frames()
            metrics = assert_full_sequence(frames, stages)
            report = {
                "target": "STM32H743 (Cortex-M7)",
                "passed": True,
                "renode": subprocess.check_output(
                    [RENODE, "--version"], text=True
                ).splitlines()[0],
                "compiler": subprocess.check_output(
                    [os.environ.get("ARM_GCC", "arm-none-eabi-gcc"), "--version"],
                    text=True,
                ).splitlines()[0],
                "elf_sha256": {
                    n: hashlib.sha256(
                        (ROOT / f"build/{n}.elf").read_bytes()
                    ).hexdigest()
                    for n in ("monitor", "receiver")
                },
                "sensor_transactions": counts,
                "metrics": metrics,
                "stages": stages,
                "frames": frames,
            }
            (ROOT / "evidence/integration.json").write_text(
                json.dumps(report, indent=2) + "\n"
            )
            trace = uart_text("receiver") + "\nMONITOR CONSOLE\n" + uart_text("monitor")
            (ROOT / "evidence/demo.txt").write_text(trace)
            print(trace)
            print("PASS full ARM firmware integration:", json.dumps(metrics))
    finally:
        if e is not None:
            e.close()


if __name__ == "__main__":
    (ROOT / "build").mkdir(exist_ok=True)
    with (ROOT / "build/integration.lock").open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise SystemExit(
                "Another integration run owns build logs; wait for it to finish."
            )
        main()
