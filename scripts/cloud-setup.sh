#!/usr/bin/env bash
# Install the C build dependencies on Ubuntu/Debian (e.g. a Claude Code
# cloud environment, which runs setup scripts as root on Ubuntu 24.04).
# Safe to run repeatedly: it does nothing if the libraries are present.
set -euo pipefail

if command -v pkg-config >/dev/null 2>&1 &&
   pkg-config --exists libcurl libcjson; then
  echo "Build dependencies already installed."
  exit 0
fi

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

$SUDO apt-get update -y
$SUDO apt-get install -y --no-install-recommends \
  build-essential pkg-config libcurl4-openssl-dev libcjson-dev ca-certificates

echo "Build dependencies installed."
