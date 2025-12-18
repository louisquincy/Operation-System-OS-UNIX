#!/bin/sh

set -eu

if [ $# -ne 1 ]; then
    echo "Error: there must be only 1 argument" >&2
    exit 1
fi

SOURCE_FILE="$1"
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: file '$SOURCE_FILE' not found" >&2
    exit 2
fi

SOURCE_DIR=$(pwd)
TEMP_DIR=$(mktemp -d)

cleanDir() {
    rm -rf "$TEMP_DIR"
    exit "$1"
}

trap 'cleanDir 130' INT
trap 'cleanDir 143' TERM
trap 'cleanDir $?' EXIT

OUTPUT=$(grep '&Output:' "$SOURCE_FILE" | sed 's/.*&Output:\s*//')
if [ -z "$OUTPUT" ]; then
    echo "Error: &Output: not found in file" >&2
    cleanDir 3
fi

SOURCE_NAME=$(basename "$SOURCE_FILE")

cp "$SOURCE_FILE" "$TEMP_DIR/" || cleanDir 4
cd "$TEMP_DIR" || cleanDir 5


case "$SOURCE_NAME" in
    *.c)
        echo "Compiling C..." >&2
        gcc "$SOURCE_NAME" -o "$OUTPUT" || cleanDir 6
        ;;
    *.cpp)
        echo "Compiling C++..." >&2
        g++ "$SOURCE_NAME" -o "$OUTPUT" || cleanDir 7
        ;;
    *.tex)
        echo "Compiling LaTeX..." >&2
        pdflatex "$SOURCE_NAME" >/dev/null 2>&1 || cleanDir 8
        pdflatex "$SOURCE_NAME" >/dev/null 2>&1
        mv "${SOURCE_NAME%.tex}.pdf" "$OUTPUT" || cleanDir 9
        ;;
    *)
        echo "Error: unsupported file type" >&2
        cleanDir 10
        ;;
esac

if [ ! -f "$OUTPUT" ]; then
    echo "Error: output file was not created" >&2
    cleanDir 11
fi

cp "$OUTPUT" "$SOURCE_DIR/" || cleanDir 12

echo "Success: $OUTPUT created in $SOURCE_DIR" >&2
cleanDir 0