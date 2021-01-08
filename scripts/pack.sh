#!/bin/bash

SRC="$(cd "$(dirname $0)/../" && pwd)"
PACKAGE_DIR="$SRC/package"
GROUP=9
mkdir -p "$PACKAGE_DIR"

cp grpwk20/enc.c "$PACKAGE_DIR/enc_$GROUP.c"
cp grpwk20/dec.c "$PACKAGE_DIR/dec_$GROUP.c"
cp grpwk20/conf.txt "$PACKAGE_DIR/conf_$GROUP.txt"
