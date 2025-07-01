#!/bin/sh

set -xe

SRC=src
BIN=bin
CFLAGS="-Wall -Wextra -pedantic -ggdb"

if [ ! -d "$SRC" ];
then 
  echo "Where is your source code" && exit 1
fi

if [ -d "$BIN" ];
then
  rm -rf "$BIN"
fi

mkdir -p "$BIN"

gcc -o ${BIN}/main ${SRC}/main.c ${SRC}/picohttpparser.c ${SRC}/http_helpers.c ${CFLAGS}

# only works on a single file
# CFLAGS="-fverbose-asm -S"
# gcc -o ${BIN}/main.s ${SRC}/main.c ${SRC}/picohttpparser.c ${CFLAGS}
# gcc ${SRC}/main.c ${SRC}/picohttpparser.c ${CFLAGS}

exit 0
