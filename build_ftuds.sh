#!/bin/sh

echo "<build>start building the application......"
rm -rf release
mkdir -p release/ipApply_ftuds_pgb/bin
mkdir -p release/ipApply_ftuds_pgb/lib
mkdir -p release/ipApply_ftuds_rca/bin
mkdir -p release/ipApply_ftuds_rca/lib
chmod 777 release

cp -r module_install.sh release/ipApply_ftuds_pgb
cp -r module_install.sh release/ipApply_ftuds_rca

cd ipApply
make rebuild PLATFORM=CPFTUDS_PGB
mv ./ipApply ../release/ipApply_ftuds_pgb/bin
cp -f ip_config.ini ../release/ipApply_ftuds_pgb/bin
cp -f ipconfig_mode ../release/ipApply_ftuds_pgb/bin
cp -f S51_ARMipApply ../release/ipApply_ftuds_pgb/bin
cp -f ipApply.sh ../release/ipApply_ftuds_pgb/bin

make rebuild PLATFORM=FTUDS_RCA
mv ./ipApply ../release/ipApply_ftuds_rca/bin
cp -f ip_config.ini ../release/ipApply_ftuds_rca/bin
cp -f ipconfig_mode ../release/ipApply_ftuds_rca/bin
cp -f S51_ARMipApply ../release/ipApply_ftuds_rca/bin
cp -f ipApply.sh ../release/ipApply_ftuds_rca/bin

cd ../libipapply
make rebuild PLATFORM=ARM64
cp -f *.so ../release/ipApply_ftuds_pgb/lib
cp libip_config.ini ../release/ipApply_ftuds_pgb/bin
cp libipconfig_mode ../release/ipApply_ftuds_pgb/bin
cp -f *.so ../release/ipApply_ftuds_rca/lib
cp libip_config.ini ../release/ipApply_ftuds_rca/bin
cp libipconfig_mode ../release/ipApply_ftuds_rca/bin
