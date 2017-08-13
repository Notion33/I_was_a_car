#! /bin/bash

echo "Burnflash Start!" 
echo "Please choose Mode : 1.Burning  2.Restore"

read mode

if [ $mode -eq 1 ]
then
	echo "Burning mode..."
	cd ../../../../
	bash find_and_replace_in_files.sh
	cd utils/scripts/burnflash
	./burnflash.sh -k quickboot_snor_linux_fs.cfg -S 31
	exit
elif [ $mode -eq 2 ]
then
	echo "Restroing mode..."
	cd ../../../../
	bash find_and_replace_in_files.sh
	cd utils/scripts/burnflash
	./burnflash.sh -Z lzf -S 31 -n eht0
   exit
else
	echo "Error : Wrong Input! Plese choose 1 or 2"
fi

