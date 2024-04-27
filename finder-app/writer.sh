#!/bin/bash

if [ "$#" != "2" ]; then 
  echo "usage: $0 file findstr";
  exit 1;
fi
dir=$(dirname $1)

mkdir -p $dir

echo $2 > $1


