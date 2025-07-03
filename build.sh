#!/bin/sh

set -xe

HTML=html
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

for file in $(ls "$HTML");
do
  case "$file" in
    *.html) xxd -i ${HTML}/${file} > ${SRC}/${file}.h &
  esac
done

wait # making headers form html files

gcc -o ${BIN}/main ${SRC}/main.c ${SRC}/picohttpparser.c ${SRC}/http_helpers.c ${CFLAGS}

exit 0
