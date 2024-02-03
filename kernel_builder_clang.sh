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

KSUVER="11986"

if [ ! -d "KernelSU" ]; then
  git clone https://github.com/backslashxx/KernelSU -b $KSUVER
fi

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

#echo "generating fake ikconfig"
#rm -rf fake
#mkdir fake
#ARCH=arm64 scripts/kconfig/merge_config.sh -O "fake" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config > /dev/null 2>&1 
#sed -i 's/Automatically generated file; DO NOT EDIT./THIS IKCONFIG IS FAKE. DO NOT BELIEVE IT./g' fake/.config
#sed -i '/is not set/s/# /# WARNING THIS IKCONFIG IS FAKE - /g' fake/.config
#mv fake/.config fake/fake.config

echo "building"
mkdir "out_$DEVICENAME"

if [ "$DEVICENAME" == "daikura" ]; then 
ARCH=arm64 scripts/kconfig/merge_config.sh -O "out_$DEVICENAME" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config arch/arm64/configs/xiaomi/sakura.config arch/arm64/configs/xiaomi/daisy.config lineageos_xx_append
elif [ "$DEVICENAME" != "daikura" ]; then
	if ! [ -f arch/arm64/configs/xiaomi/"$DEVICENAME".config ]; then
  	echo arch/arm64/configs/xiaomi/"$DEVICENAME".config doesnt exist
  	exit
	fi
ARCH=arm64 scripts/kconfig/merge_config.sh -O "out_$DEVICENAME" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config arch/arm64/configs/xiaomi/"$DEVICENAME".config lineageos_xx_append
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
echo "ksu: $KSUVER"

# fp asimd evtstrm aes pmull sha1 sha2 crc32
# for i in $(ls patches/) ; do patch -Np1 < patches/$i ; done
