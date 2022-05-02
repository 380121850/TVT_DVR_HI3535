
TOOLS_DIR=$(OSDRV_DIR)/tools/board_tools/parted/

all:
	rm parted-3.0 -rf;
	tar -zxf parted-3.0-hisi-bvt.tar.gz;
	mkdir -p $(TOOLS_DIR)/parted-3.0/bin/;
	pushd $(TOOLS_DIR)/parted-3.0/;\
		./configure --host=arm-linux --enable-static --disable-shared CC=$(OSDRV_CROSS)-gcc AR=$(OSDRV_CROSS)-ar LD=$(OSDRV_CROSS)-ld\
		--disable-device-mapper --without-readline --prefix=$(TOOLS_DIR)/parted-3.0/bin > /dev/null;\
		make -j 20 > /dev/null;make install > /dev/null;\
	popd

clean:
	rm parted-3.0 -rf;
