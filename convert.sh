#!/usr/bin/bash

docs=docs
files=$( ls $docs )

for file in $files; do
    markdown-it $docs/$file > $docs/$file.html
done
