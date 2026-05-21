#!/bin/sh

echo "<build>start building the dynamic library......"
make rebuild PLATFORM=PC
mkdir release
mkdir release/lib
mkdir release/include
cp -f *.so release/lib
cp libipapply.h release/include
cp ip_config.ini release/
cp ipconfig_mode release/
echo "<build>build completed!"