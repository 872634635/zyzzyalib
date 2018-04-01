#!/bin/sh

nu=`ps -A | awk '{if($4=="run") printf $1}'`
kill -9 $nu
