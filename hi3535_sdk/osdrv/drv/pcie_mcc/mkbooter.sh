#! /bin/sh

dd if=./out/booter of=./tmp1 bs=16384 conv=sync
dd if=./out/pc_jump.bin of=./tmp2 bs=512 conv=sync
dd if=./out/ddr_training.bin of=./tmp3 bs=7680 conv=sync
cat tmp1 tmp2 tmp3 > ./out/booter
rm -f tmp1 tmp2 tmp3

