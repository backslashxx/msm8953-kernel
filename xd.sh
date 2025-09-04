#!/bin/bash

# shitty kernel reeeee

#DEVICENAME=daisy

echo $DEVICENAME | egrep "daikura|daisy|sakura|ysl" || (echo not testes && exit)

#if ! [ -f arch/arm64/configs/xiaomi/"$DEVICENAME".config ]; then
#  echo arch/arm64/configs/xiaomi/"$DEVICENAME".config doesnt exist
#  exit
#fi



#PREFIX="$(pwd)"
PREFIX="/tmp/optane/clang"

export ARCH=arm64
export SUBARCH=arm64
export HEADER_ARCH=arm64


# Garbage removal

#rm -rf out
#mkdir out
#rm -rf error.log
#make O=out clean 
#make mrproper


# Build

CLANG_DIR=${PREFIX}


export PATH="$CLANG_DIR/bin:$PATH"

#echo $PATH

echo "building"
mkdir "out_$DEVICENAME"

echo "CONFIG_LOCALVERSION=\""-$DEVICENAME\""" > /tmp/lineageos_xx_devicename

if [ "$DEVICENAME" == "daikura" ]; then 
ARCH=arm64 scripts/kconfig/merge_config.sh -O "out_$DEVICENAME" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config arch/arm64/configs/xiaomi/sakura.config arch/arm64/configs/xiaomi/daisy.config
elif [ "$DEVICENAME" != "daikura" ]; then
	if ! [ -f arch/arm64/configs/xiaomi/"$DEVICENAME".config ]; then
  	echo arch/arm64/configs/xiaomi/"$DEVICENAME".config doesnt exist
  	exit
	fi
ARCH=arm64 scripts/kconfig/merge_config.sh -O "out_$DEVICENAME" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config arch/arm64/configs/xiaomi/"$DEVICENAME".config
fi


make -j24 ARCH=arm64 SUBARCH=arm64 O=out_$DEVICENAME \
        CC="ccache clang"\
	AS="clang" \
        AR="llvm-ar" \
	NM="llvm-nm" \
	LD="ld.lld" \
	OBJCOPY="llvm-objcopy" \
	OBJDUMP="llvm-objdump" \
	STRIP="llvm-strip" \
        CLANG_TRIPLE="aarch64-linux-gnu-" \
    	CROSS_COMPILE="aarch64-linux-gnu-" \
    	CROSS_COMPILE_ARM32="arm-linux-gnueabi-" \
    	CROSS_COMPILE_COMPAT="arm-linux-gnueabi-" \
    	LLVM=1 \
    	LLVM_IAS=1 \
    	INSTALL_MOD_STRIP=1 \
	KBUILD_BUILD_USER="$(git rev-parse --short HEAD | cut -c1-7)" \
	KBUILD_BUILD_HOST="$(git symbolic-ref --short HEAD)" \
	KBUILD_BUILD_FEATURES="source: https://github.com/backslashxx/msm8953-kernel //"

ccache -s

rm /tmp/lineageos_xx_devicename

# fp asimd evtstrm aes pmull sha1 sha2 crc32
# for i in $(ls patches/) ; do patch -Np1 < patches/$i ; done
