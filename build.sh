#!/bin/sh

set -xe

SRC=src
BIN=bin
CLAGS="-Wall -Wextra -pedantic -ggdb"

if [ ! -d "$SRC" ];
then 
  echo "Where is your source code" && exit 1
fi

if [ -d "$BIN" ];
then
  rm -rf "$BIN"
fi

mkdir -p "$BIN"

gcc -o ${BIN}/main ${SRC}/main.c ${CLAGS}

exit 0
