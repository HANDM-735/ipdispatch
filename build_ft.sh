#!/bin/sh

echo "<build>start building the application......"
rm -rf release
mkdir -p release/ipApply_ftsync/bin
mkdir -p release/ipApply_ftsync/lib
mkdir -p release/ipApply_ftpgb/bin
mkdir -p release/ipApply_ftpgb/lib
chmod 777 release

cp -r module_install.sh release/ipApply_ftsync
cp -r module_install.sh release/ipApply_ftpgb

cd ipApply
make rebuild PLATFORM=FT_SYNC
mv ./ipApply ../release/ipApply_ftsync/bin
cp -f ip_config.ini ../release/ipApply_ftsync/bin
cp -f ipconfig_mode ../release/ipApply_ftsync/bin
cp -f ipApply.service ../release/ipApply_ftsync/bin
cp -f ipApply.sh ../release/ipApply_ftsync/bin

make rebuild PLATFORM=FT_PGB
mv ./ipApply ../release/ipApply_ftpgb/bin
cp -f ip_config.ini ../release/ipApply_ftpgb/bin
cp -f ipconfig_mode ../release/ipApply_ftpgb/bin
cp -f ipApply.service ../release/ipApply_ftpgb/bin
cp -f ipApply.sh ../release/ipApply_ftpgb/bin

cd ../libipapply
make rebuild PLATFORM=FT_SYNC
cp -f *.so ../release/ipApply_ftsync/lib
cp libip_config.ini ../release/ipApply_ftsync/bin
cp libipconfig_mode ../release/ipApply_ftsync/bin

make rebuild PLATFORM=FT_PGB
cp -f *.so ../release/ipApply_ftpgb/lib
cp libip_config.ini ../release/ipApply_ftpgb/bin
cp libipconfig_mode ../release/ipApply_ftpgb/bin
