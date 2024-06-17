#!/bin/bash

if [ "$#" != "2" ]; then 
  echo "usage: $0 file string";
  exit 1;
fi
dir=$(dirname $1)

mkdir -p $dir

echo $2 > $1


