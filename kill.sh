#!/bin/sh

nu=`ps -A | awk '{if($4=="serverp") printf $1}'`
kill -9 $nu
