#!/bin/sh

filesdir=$1
searchstr=$2

if [ $# -lt 2 ]; then
    echo "Valid input arguments needed!"
    exit 1
fi

if [ ! -d "$filesdir" ]; then
   echo "Wrong file directory entered"
   exit 1
fi

number_of_files=`find $filesdir -type f | wc -l`

number_of_lines=`grep -r $searchstr $filesdir | wc -l`

echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"