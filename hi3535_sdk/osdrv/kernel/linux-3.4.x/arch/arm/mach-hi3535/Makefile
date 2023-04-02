#
# Makefile for the linux kernel.
#


KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=softvfp+vfp)
LDFLAGS		+=--no-warn-mismatch

obj-y					:= core.o clock.o timer.o
obj-$(CONFIG_SYNO_HI3535_VS)            += synology-gpio.o
obj-$(CONFIG_SYNO_HI3535_NVR)           += synology-gpio.o
obj-$(CONFIG_SMP)                       += platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)               += hotplug.o
obj-$(CONFIG_HW_PERF_EVENTS)            += pmu.o
obj-$(CONFIG_SYNO_GPIO_DEVICE)          += syno_gpio_device.o
obj-$(CONFIG_SYNO_I2C_DEVICE)           += syno_i2c_device.o
