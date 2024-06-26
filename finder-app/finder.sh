#!/bin/sh
#The number of files are X and the number of matching lines are Y
if [ "$#" != "2" ]; then 
  echo "usage: $0 file findstr";
  exit 1;
fi

count=$(grep $2 $1 -r | wc -l)
echo "The number of files are $count and the number of matching lines are $count"


