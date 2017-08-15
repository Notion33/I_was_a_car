#! /bin/bash

argc=$#
argv0=$0
argv1=$1
argv2=$2
argv3=$3
argv4=$4

if [ $argv1 = -o ]
then
	gcc `pkg-config opencv --cflags` $argv3 -o $argv2 `pkg-config opencv --libs`
	exit

elif [ $argv2 = -o ]
then
	gcc `pkg-config opencv --cflags` $argv1 -o $argv3 `pkg-config opencv --libs`
	exit

elif [ $argc -eq 1 ]
then
	gcc `pkg-config opencv --cflags` $argv1 `pkg-config opencv --libs`
	exit

else
	echo "Error : wrong input."
	echo "Plese input as [a.c -o a.o] or [-o a.o a.c]"
fi
