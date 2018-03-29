#!/bin/sh

nu=`ps -A | awk '{if($4=="service") print $1}'`
if [!$nu]
then
	echo "not exsit servicep process"
else
	echo "kill this process"
	kill -9 $nu
fi

git fetch origin
git merge origin/master
cp ./servicep ./clientp ../
cd ..
./servicep
