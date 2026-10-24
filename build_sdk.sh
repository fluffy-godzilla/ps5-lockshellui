#!/bin/bash
# main build script, expects to be run from the root of the repository and to be provided the version number 
# as an argument (e.g. ./build.sh 1.2.3)
VERSION=${1:-"1.0.0"}  # Default version if not set in environment

ELFNAME="lockshellui"
INPUT_ELF="${ELFNAME}.elf"
OUTPUT_ELF="${ELFNAME}_v${VERSION}.elf"
IMAGE_NAME="ps5-payload-sdk-${ELFNAME}"

echo "--- Building $OUTPUT_ELF ---"

# 1. Build/verify the docker image
echo "[1/3] Building Docker Image..."
if [[ "$(docker images -q $IMAGE_NAME 2> /dev/null)" == "" ]]; then
    echo "      Docker image $IMAGE_NAME not found. Building... (this may take a few minutes)"
    docker build -t $IMAGE_NAME -f Dockerfile.sdk .
    if [ $? -ne 0 ]; then
        echo "      !!! Docker image build FAILED!"
        exit 1
    fi
    echo "      Docker image built successfully."
fi

# 2. Build native ELF via Docker
echo "[2/3] Building native ELF via Docker..."
docker run --rm -v "$(pwd)":/src -w /src $IMAGE_NAME make -f Makefile.sdk   clean all > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "      !!! ELF build FAILED!"
    exit 1
fi
echo "      ELF build successful."

# 3. Rename output
if [ -f "$INPUT_ELF" ]; then
    mv "$INPUT_ELF" "$OUTPUT_ELF"
    echo "[3/3] Created versioned binary: $OUTPUT_ELF"
    echo "--- Build Complete! ---"
else
    echo "      !!! $INPUT_ELF not found after build!"
    exit 1
fi
