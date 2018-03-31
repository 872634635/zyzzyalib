#!/bin/sh

nserver=`ps -A | awk '{if($4=="server") print $1}'`
bg $nu
nu=`ps -A | awk '{if($4=="serverp") print $1}'`
if [!$nu]
then
	echo "not exsit servicep process"
else
	echo "kill this process"
	kill -9 $nu
fi

cd ..

./serverp



