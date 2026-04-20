#!/bin/sh

echo "<build>start building the application......"
rm -rf release
mkdir release
mkdir release/bin
chmod 777 release

cd ipReply
make rebuild PLATFORM=PC
mv ./ipReply ../release/bin
cp -f ipReply.service ../release/bin
cp -f ipReply.sh ../release/bin
