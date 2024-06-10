#!/usr/bin/bash

set -e

if [[ $1 != "" ]]; then
    docs=$1
else
    docs=docs
fi

files=$( ls $docs )

for file in $files; do
    markdown-it $docs/$file > $docs/$file.html
done
