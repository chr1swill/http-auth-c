#!/bin/sh

set -xe

CC=gcc
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

html_newer_than_header() {
  html_file="$1"
  case "$html_file" in
    *.html);;
    *)
      echo "positional parameter 1 must be a *.html file" && \
        exit 1
    ;;
  esac

  header_file="$2"
  case "$header_file" in
    *.h);;
    *)
      echo "positional parameter 2 must be a *.h file" && \
        exit 1
    ;;
  esac

  html_file_time=$(stat -t --format="%Y" ${html_file} 2> /dev/null || "0")
  header_file_time=$(stat -t --format="%Y" ${header_file} 2> /dev/null || "0")

  if [ "$html_file_time" -eq "0" ];
  then
    echo "this should never happend" && exit 1
  fi

  if [ "$html_file_time" -gt "$header_file_time" ];
  then
    echo "1"
  else
    echo "0"
  fi
}

for file in $(ls "$HTML");
do
  case "$file" in
    *.html) 
      if [ -f "${SRC}/${file}.h" ];
      then
        html_file_is_newer=$(html_newer_than_header "${HTML}/${file}" "${SRC}/${file}.h")
        if [ "$html_file_is_newer" -eq "1" ];
        then
          xxd -i ${HTML}/${file} > ${SRC}/${file}.h &
        fi
      else
        xxd -i ${HTML}/${file} > ${SRC}/${file}.h &
      fi
  esac
done

# wait for all header to be build before compiling
wait 

${CC} -o ${BIN}/main ${SRC}/main.c ${SRC}/picohttpparser.c ${CFLAGS}

exit 0
