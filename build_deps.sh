#!/usr/bin/env bash
# Dependency build script for building inside Docker
set -e

TEMPDIR=$(mktemp -d)
trap 'rm -rf -- "$TEMPDIR"' EXIT

cd $TEMPDIR

echo "=== 1. Handling sqlite3 3.54.1 ==="
wget -O sqlite-amalgamation.zip https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
unzip -q -j sqlite-amalgamation.zip "*/sqlite3.*" -d sqlite3

mkdir -p /opt/ps5-payload-sdk/target/src/
cp -r sqlite3/*.c /opt/ps5-payload-sdk/target/src/
cp -r sqlite3/*.h /opt/ps5-payload-sdk/target/include/

echo "All dependencies (sqlite3) successfully built and/or installed!"