#!/bin/bash
# normalize old-style return(X) statements to return X;

perl -pi -e 's/\breturn\s*\(\s*([^)]+?)\s*\)\s*;/return $1;/g' *.c
