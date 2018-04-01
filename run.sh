#!/bin/sh

nu=`ps -A | awk '{if($4=="run") print $1}'`
if [!$nu]
then
	echo "not exsit servicep process"
else
	echo "kill this process"
	kill -9 $nu
fi

./run



