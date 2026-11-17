#!/usr/bin/env python3
"""Plot receiver observations only, using artifacts from a passing integration test."""

import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]


def main():
    report = json.loads((ROOT / "evidence/integration.json").read_text())
    assert report["passed"], "A successful emulator integration run is required"
    f = report["frames"]
    t = [x["t"] / 1000 for x in f]
    plt.rcParams.update(
        {
            "font.family": "DejaVu Sans",
            "font.size": 10,
            "svg.hashsalt": "stm32-health-monitor",
        }
    )
    fig, axes = plt.subplots(
        3,
        1,
        figsize=(11, 6.8),
        sharex=True,
        layout="constrained",
        height_ratios=[1, 1, 1],
    )
    fig.suptitle(
        "STM32H743: telemetry decoded by the second emulated MCU", fontweight="bold"
    )
    axes[0].step(
        t,
        [x["vibration"] for x in f],
        where="post",
        color="#1769aa",
        label="max(|X|, |Y|)",
    )
    # Symmetric log keeps the 32768mg signed edge case and the 400mg threshold visible.
    axes[0].set_yscale("symlog", linthresh=500)
    axes[0].set_ylabel("Vibration (mg)")
    axes[0].set_yticks([0, 400, 900, 32768], ["0", "400", "900", "32768"])
    axes[0].legend(loc="upper left")
    axes[1].step(
        t,
        [x["temp"] / 10 for x in f],
        where="post",
        color="#00897b",
        label="Last valid temperature",
    )
    axes[1].set_ylabel("Temperature (°C)")
    axes[1].legend(loc="upper right")
    axes[2].step(
        t,
        [1.2 + (x["flags"] & 1) for x in f],
        where="post",
        label="Vibration alarm",
        color="#d1495b",
    )
    axes[2].step(
        t,
        [(x["flags"] >> 1) & 1 for x in f],
        where="post",
        label="Temperature fault",
        color="#8e44ad",
    )
    axes[2].set_yticks([0, 1, 1.2, 2.2], ["clear", "FAULT", "clear", "ALARM"])
    axes[2].set_ylim(-0.15, 2.45)
    axes[2].set_ylabel("Status bits")
    axes[2].legend(loc="upper right")
    axes[2].set_xlabel("Receiver SysTick virtual time (s)")
    outage = next(
        s for s in report["stages"] if s["name"] == "temperature missing response"
    )
    restoration = next(
        s for s in report["stages"] if s["name"] == "temperature restored"
    )
    for ax in axes:
        ax.axvspan(
            outage["start_ms"] / 1000,
            restoration["start_ms"] / 1000,
            color="#efb366",
            alpha=0.18,
        )
        ax.grid(alpha=0.2)
        ax.set_xlim(0, report["metrics"]["duration_ms"] / 1000)
        ax.scatter(
            t,
            [x["vibration"] for x in f]
            if ax is axes[0]
            else [x["temp"] / 10 for x in f]
            if ax is axes[1]
            else [(x["flags"] >> 1) & 1 for x in f],
            s=12,
            zorder=3,
        )
    axes[1].text(
        1.18,
        5,
        "Sensor unavailable\nUART and CAN continue",
        fontsize=9,
        color="#6e4713",
    )
    fig.savefig(
        ROOT / "evidence/demo.svg",
        metadata={
            "Date": None,
            "Creator": "STM32 Machine Health Monitor tools/plot_trace.py",
        },
    )
    print("Saved evidence/demo.svg from", len(f), "actual receiver CAN frames")


if __name__ == "__main__":
    main()
