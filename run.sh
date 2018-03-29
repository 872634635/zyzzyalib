#!/bin/sh

make clean
make

cd ../simple_zyzva

make clean
make

./servicep

cd ./gitzyva
mv ../clientp ../servicep ./
git add ./clientp ./servicep
git commit -m 'temp'
git push origin master



