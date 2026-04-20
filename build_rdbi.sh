#!/bin/sh

echo "<build>start building the application......"
rm -rf release
mkdir release
mkdir release/bin
mkdir release/lib
chmod 777 release

cd ipApply
make rebuild PLATFORM=ARM64_RDBI
mv ./ipApply ../release/bin
cp -f ip_config.ini ../release/bin
cp -f ipconfig_mode ../release/bin
cp -f S51ipApply ../release/bin

cd ../libipapply
make rebuild PLATFORM=ARM64_RDBI
cp -f *.so ../release/lib
cp libip_config.ini ../release/lib
cp libipconfig_mode ../release/lib
