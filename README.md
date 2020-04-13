Before using the sources, please read this:

1. Use them at your own responsibility.

3. All of the software is tested by me, but I am not responsible for any issues that can occure during working.

4. I am not responsible for any damages on your board, PC, external modules or any hardware you are using.

CROSS COMPILATION for Raspbian kernel 4.9.y (thanks to www.grendelman.net)
on your host x64 machine terminal:

cd

mkdir ~/raspberrypi

cd ~/raspberrypi

git clone https://github.com/raspberrypi/linux.git

git clone https://github.com/raspberrypi/tools.git

cd linux

make -j n ARCH=arm CROSS_COMPILE=~/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf- bcm2709_defconfig

make -j n ARCH=arm CROSS_COMPILE=~/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-

OR https://www.raspberrypi.org/documentation/linux/kernel/building.md - use same locations on the host

Now you have a cross compiled linux kernel (in ~/raspberrypi/linux) and cross compilation tools (in ~/raspberrypi/tools).
To compile modules against it you just need to use Makefiles.onhost(do not forget to specify the module name in Makefile).

ONBOARD COMPILATION for Raspbian - see pandalion98's answer in:

http://raspberrypi.stackexchange.com/questions/39845/how-compile-a-loadable-kernel-module-without-recompiling-kernel

After you upgrade your kernel version and install sources, use Makefile.(do not forget to specify the module name in Makefile).
