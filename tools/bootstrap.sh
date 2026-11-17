#!/usr/bin/env bash
# Install pinned portable tools into this repository. Linux x86-64 only.
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
tool_dir="${TOOLS_DIR:-$repo_dir/.tools}"
case "$(uname -sm)" in 'Linux x86_64') ;; *) echo 'Portable archives require Linux x86-64.' >&2; exit 1 ;; esac
mkdir -p "$tool_dir"
fetch() {
    local url="$1" name="$2" checksum="$3" unpacked="$4"
    if [[ -x "$tool_dir/$unpacked" ]]; then return; fi
    curl --fail --location --retry 3 "$url" --output "$tool_dir/$name"
    printf '%s  %s\n' "$checksum" "$tool_dir/$name" | sha256sum --check
    tar -xzf "$tool_dir/$name" -C "$tool_dir"
    test -x "$tool_dir/$unpacked"
}
fetch 'https://github.com/renode/renode/releases/download/v1.17.0/renode-1.17.0.linux-portable.tar.gz' \
    renode.tar.gz 4ba7c68b59e2447f188ef4b4b112fcccd2802582c460beedceb63b84e5605a5f \
    renode_1.17.0-portable/renode
fetch 'https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz' \
    arm-gcc.tar.gz da6a49ad4003944b823c6c93702a8787c922ab34bd7e918ec0eaf6933a9b1ff6 \
    xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc
"$tool_dir/renode_1.17.0-portable/renode" --version
"$tool_dir/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin/arm-none-eabi-gcc" --version | head -1
printf 'Tools installed in %s\n' "$tool_dir"
