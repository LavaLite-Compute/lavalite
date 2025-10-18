#!/bin/bash

files=`\ls -l|grep -v total|awk '{print $NF}'`
for f in $files
do
  echo "$f \\"
done

