#!/bin/bash
#
# Copyright 2004-present Facebook. All Rights Reserved.
#
CHECKOUT_DIR=$1
INSTALL_DIR=$2
if [[ -z $INSTALL_DIR ]]; then
  echo "usage: $0 CHECKOUT_DIR INSTALL_DIR" >&2
  exit 1
fi

# Convert INSTALL_DIR to an absolute path so it still refers to the same
# location after we cd into the build directory.
case "$INSTALL_DIR" in
  /*) ;;
  *) INSTALL_DIR="$PWD/$INSTALL_DIR"
esac

# If folly was already installed, just return early
INSTALL_MARKER_FILE="$INSTALL_DIR/folly.installed"
if [[ -f $INSTALL_MARKER_FILE ]]; then
  echo "folly was previously built"
  exit 0
fi

set -e
set -x

if [[ -d "$CHECKOUT_DIR" ]]; then
  git -C "$CHECKOUT_DIR" fetch
  git -C "$CHECKOUT_DIR" checkout master
else
  git clone https://github.com/facebook/folly "$CHECKOUT_DIR"
fi

mkdir -p "$CHECKOUT_DIR/_build"
cd "$CHECKOUT_DIR/_build"
if ! cmake \
    "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}" \
    "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" \
    ..; then
  echo "error configuring folly" >&2
  tail -n 100 CMakeFiles/CMakeError.log >&2
  exit 1
fi
make -j4
make install
touch "$INSTALL_MARKER_FILE"
