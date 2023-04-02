lzo_compress-objs   := lzma_compress.o
lzo_decompress-objs := lzma_decompress.o

EXTRA_CFLAGS += -Wall -D_7ZIP_ST
obj-$(CONFIG_LZO_COMPRESS) += lzmaenc.o lzfind.o
obj-$(CONFIG_LZO_DECOMPRESS) += lzmadec.o
