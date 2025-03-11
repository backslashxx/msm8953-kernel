#!/bin/bash

# shitty kernel reeeee

#DEVICENAME=daisy

echo $DEVICENAME | egrep "daikura|daisy|sakura|ysl" || (echo not testes && exit)

#if ! [ -f arch/arm64/configs/xiaomi/"$DEVICENAME".config ]; then
#  echo arch/arm64/configs/xiaomi/"$DEVICENAME".config doesnt exist
#  exit
#fi

PREFIX="/tmp/optane/gcc"
GCC64="openwrt-toolchain-qualcommax-ipq807x_gcc-13.3.0_musl.Linux-x86_64/toolchain-aarch64_cortex-a53_gcc-13.3.0_musl"
GCC32="openwrt-toolchain-ipq40xx-generic_gcc-13.3.0_musl_eabi.Linux-x86_64/toolchain-arm_cortex-a7+neon-vfpv4_gcc-13.3.0_musl_eabi"

export PATH="$PREFIX$GCC64/bin:$PREFIX$GCC32/bin:$PATH"
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

GCC64_DIR=${PREFIX}/${GCC64}
GCC32_DIR=${PREFIX}/${GCC32}

export PATH="$GCC64_DIR/bin:$GCC32_DIR/bin:$CLANG_DIR/bin:$PATH"

#echo $PATH

echo "building"
mkdir "out_$DEVICENAME"

if [ "$DEVICENAME" == "daikura" ]; then 
ARCH=arm64 scripts/kconfig/merge_config.sh -O "out_$DEVICENAME" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config arch/arm64/configs/xiaomi/sakura.config arch/arm64/configs/xiaomi/daisy.config lineageos_xx_append lineageos_xx_vdso
elif [ "$DEVICENAME" != "daikura" ]; then
	if ! [ -f arch/arm64/configs/xiaomi/"$DEVICENAME".config ]; then
  	echo arch/arm64/configs/xiaomi/"$DEVICENAME".config doesnt exist
  	exit
	fi
ARCH=arm64 scripts/kconfig/merge_config.sh -O "out_$DEVICENAME" arch/arm64/configs/msm8953-perf_defconfig arch/arm64/configs/xiaomi/xiaomi.config arch/arm64/configs/xiaomi/"$DEVICENAME".config lineageos_xx_append lineageos_xx_vdso
fi


make -j24 ARCH=arm64 SUBARCH=arm64 O="out_$DEVICENAME" \
        CROSS_COMPILE="ccache aarch64-openwrt-linux-" \
        CROSS_COMPILE_ARM32="ccache arm-openwrt-linux-" \
        CROSS_COMPILE_COMPAT="ccache arm-openwrt-linux-" \
        INSTALL_MOD_STRIP=1 \
	KBUILD_BUILD_USER="$(git rev-parse --short HEAD | cut -c1-7)" \
	KBUILD_BUILD_HOST="$(git symbolic-ref --short HEAD)" \
	KBUILD_BUILD_FEATURES="source: https://github.com/backslashxx/msm8953-kernel //"

ccache -s

# fp asimd evtstrm aes pmull sha1 sha2 crc32
# for i in $(ls patches/) ; do patch -Np1 < patches/$i ; done
