./configure --prefix=/docker/opt/dsm/TVT_DVR_HI3535/hi3535_sdk/osdrv/minigui/install \
CC=arm-hisiv200-linux-gcc --host=arm-hisiv200-linux --disable-pcxvfb --disable-screensaver \
--disable-splash --disable-jpgsupport --enable-videoqvfb=no --enable-rtosxvfb=no --enable-pcxvfb=no

./configure --prefix=/docker/opt/dsm/TVT_DVR_HI3535/hi3535_sdk/osdrv/minigui/install \
CC=arm-hisiv200-linux-gcc --host=arm-hisiv200-linux


./configure --prefix=/docker/opt/dsm/TVT_DVR_HI3535/hi3535_sdk/osdrv/minigui/install CC=arm-hisiv200-linux-gcc \
--host=arm-hisiv200-linux --disable-pcxvfb --disable-screensaver --disable-splash \
--disable-jpgsupport --enable-videoqvfb=no --enable-rtosxvfb=no --enable-pcxvfb=no \
--enable-videohi3535=yes