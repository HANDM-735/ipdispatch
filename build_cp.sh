#!/bin/sh

echo "<build>start building the application......"
rm -rf release
mkdir -p release/ipApply_cpsync/bin
mkdir -p release/ipApply_cpsync/lib
mkdir -p release/ipApply_cprca/bin
mkdir -p release/ipApply_cprca/lib
chmod 777 release

cp -r module_install.sh release/ipApply_cpsync
cp -r module_install.sh release/ipApply_cprca

cd ipApply
make rebuild PLATFORM=CP_SYNC
mv ./ipApply ../release/ipApply_cpsync/bin
cp -f ip_config.ini ../release/ipApply_cpsync/bin
cp -f ipApply.service ../release/ipApply_cpsync/bin
cp -f ipApply.sh ../release/ipApply_cpsync/bin

make rebuild PLATFORM=CP_RK3588
mv ./ipApply ../release/ipApply_cprca/bin
cp -f ip_config.ini ../release/ipApply_cprca/bin
cp -f S51_ARMipApply ../release/ipApply_cprca/bin
cp -f ipApply.sh ../release/ipApply_cprca/bin

cd ../libipapply
make rebuild PLATFORM=CP_SYNC
cp -f *.so ../release/ipApply_cpsync/lib
cp libip_config.ini ../release/ipApply_cpsync/bin
cp libipconfig_mode ../release/ipApply_cpsync/bin

make rebuild PLATFORM=CP_RK3588
cp -f *.so ../release/ipApply_cprca/lib
cp libip_config.ini ../release/ipApply_cprca/bin
cp libipconfig_mode ../release/ipApply_cprca/bin
