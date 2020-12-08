#!/bin/bash
set -ex

REVISON1=$1
REVISON2=$2

SRC="$(cd "$(dirname $0)/../" && pwd)"

run_test() {
  (cd $SRC/grpwk20 && \
    make clean && make all && \
    ./test.sh $(cat $SRC/grpwk20/conf.txt | tail -n1)
  )
}

ARTIFACTS="$SRC/.artifacts"
mkdir -p $ARTIFACTS

git checkout $REVISON1 > /dev/null
run_test &> "$ARTIFACTS/$REVISON1"

git checkout $REVISON2 > /dev/null
run_test &> "$ARTIFACTS/$REVISON2"

diff "$ARTIFACTS/$REVISON1" "$ARTIFACTS/$REVISON2"
