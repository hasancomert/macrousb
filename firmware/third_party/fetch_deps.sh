#!/usr/bin/env bash
# Fetch pinned third-party dependencies for the macrousb firmware.
# Shallow clones (no history) at fixed tags for reproducible builds.
#
#   Usage:  cd firmware/third_party && ./fetch_deps.sh
#
# These directories are git-ignored; this script re-creates them.
set -euo pipefail
cd "$(dirname "$0")"

clone() { # <dir> <tag> <url>
  local dir="$1" tag="$2" url="$3"
  if [ -d "$dir/.git" ]; then
    echo "== $dir already present (skip)"; return
  fi
  echo "== cloning $dir @ $tag"
  rm -rf "$dir"
  git clone --depth 1 --branch "$tag" "$url" "$dir"
}

clone tinyusb              0.21.0                 https://github.com/hathach/tinyusb.git
clone lwip                 STABLE-2_2_1_RELEASE   https://github.com/lwip-tcpip/lwip.git
clone cmsis_core           v5.9.0                 https://github.com/STMicroelectronics/cmsis_core.git
clone cmsis_device_f4      v2.6.11                https://github.com/STMicroelectronics/cmsis_device_f4.git
clone stm32f4xx_hal_driver v1.8.5                 https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git

echo "All dependencies fetched."
