#!/usr/bin/env bash

set -e

if [ -x "$(command -v xcrun)" ]; then
    xcrun -sdk macosx metal     -o gltf.air      -c gltf.metal
    xcrun -sdk macosx metallib  -o gltf.metallib gltf.air
fi
