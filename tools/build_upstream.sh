#!/usr/bin/env bash
# Assembles the pinned UltraStarFox sources into SF.SFC and SYMBOLS.TXT using
# a native DOSBox-X. This is the macOS/Linux counterpart of build_upstream.ps1,
# which drives the Windows dosbox-x.exe that ships inside the upstream tree.
set -euo pipefail

expected_commit='270e959a47d82240d9290a6c6630032c9ec53ff5'
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_root="${1:-${script_dir}/../upstream-ultrastarfox}"
source_root="$(cd "${source_root}" && pwd)"

if ! command -v dosbox-x >/dev/null 2>&1; then
    echo "dosbox-x is required (brew install dosbox-x)" >&2
    exit 1
fi

actual_commit="$(git -C "${source_root}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${expected_commit}" ]]; then
    echo "UltraStarFox must be checked out at ${expected_commit} (found ${actual_commit})" >&2
    exit 1
fi

batch_path="${source_root}/.starfox-port-build.bat"
success_path="${source_root}/.starfox-port-build.ok"
if [[ -e "${batch_path}" ]]; then
    echo "Refusing to overwrite existing temporary build file: ${batch_path}" >&2
    exit 1
fi

cleanup() { rm -f "${batch_path}" "${success_path}"; }
trap cleanup EXIT

rm -f "${success_path}"
# BUILD.BAT ends in a pause; drive an unattended variant instead so the
# assembler run can complete without a keypress.
printf '%s\r\n' \
    '@echo off' \
    'set path=%path%;c:\bin' \
    'cd sf' \
    'make' \
    'if errorlevel 1 goto failed' \
    'cd ..' \
    'echo ok>.starfox-port-build.ok' \
    'exit' \
    ':failed' \
    'cd ..' \
    'exit' > "${batch_path}"

(cd "${source_root}" && dosbox-x -fastlaunch -nolog "$(basename "${batch_path}")")

if [[ ! -f "${success_path}" ]]; then
    echo "UltraStarFox assembler build failed" >&2
    exit 1
fi

for name in SF.SFC SYMBOLS.TXT BANKS.CSV; do
    path="${source_root}/${name}"
    if [[ ! -f "${path}" ]]; then
        echo "UltraStarFox build did not produce ${name}" >&2
        exit 1
    fi
    ls -l "${path}"
done
