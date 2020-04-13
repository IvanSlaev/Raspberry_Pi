CROSS_COMPILE := $(if $(findstring arm,$(shell uname -p)),,~/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-)


CC=${CROSS_COMPILE}gcc
SRC=${wildcard *.c}

test: $(SRC)
	$(CC) -o $@ $^

clean:
	rm test
