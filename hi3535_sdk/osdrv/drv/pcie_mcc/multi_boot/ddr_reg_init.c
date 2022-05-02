#include "../hi35xx_dev/include/hi35xx_dev.h"
#include "hios_boot_usrdev.h"
#include "ddr_reg_init.h"

static int s_current_reg_base = 0;

static inline void DWB(void) {
	;
}

static inline void delay(void)
{
	__asm__ __volatile__("nop");
}

static inline void reg_read(struct regentry *reg,
		struct hi35xx_dev *hi_dev,
		unsigned int *ret)
{
	unsigned int reg_val_r;
	unsigned int bit_start_r;
	unsigned int bit_num_r;

	bit_start_r = ((reg->attr&R_REG_BIT_MASK)>>R_REG_BIT_OFFSET);
	bit_num_r = ((reg->attr&R_BIT_MASK)>>R_BIT_OFFSET)+1;

	if (s_current_reg_base != (reg->reg_addr & 0xffff0000)) {
		boot_trace(BOOT_INFO, "read:move pf win: currentbase 0x%x, regbase 0x%x",
				s_current_reg_base, reg->reg_addr);
		s_current_reg_base =  (reg->reg_addr & 0xffff0000);

		g_local_handler->move_pf_window_in(hi_dev,
				s_current_reg_base,
				0xffff,
				0x1);
	}

	reg_val_r = (*(volatile unsigned *) (reg->reg_addr
				- s_current_reg_base
				+ hi_dev->pci_bar1_virt));

	if (32 != bit_num_r) {
		reg_val_r >>= bit_start_r;
		reg_val_r &= ((1<<bit_num_r) - 1);
	}

	*ret = ((reg_val_r == reg->value) ? 0:1);
}

static inline void reg_write(struct regentry *reg, struct hi35xx_dev *hi_dev)
{
	unsigned int reg_val_w;
	unsigned int delay_2;
	unsigned int bit_start_w;
	unsigned int bit_num_w;

	delay_2 = reg->delay;
	bit_start_w = ((reg->attr&W_REG_BIT_MASK)>>W_REG_BIT_OFFSET);
	bit_num_w = ((reg->attr&W_BIT_MASK)>>W_BIT_OFFSET) + 1;

	if (s_current_reg_base != (reg->reg_addr & 0xffff0000)) {
		boot_trace(BOOT_INFO, "write:move pf win: currentbase 0x%x, regbase 0x%x",
				s_current_reg_base, reg->reg_addr);

		s_current_reg_base =  (reg->reg_addr & 0xffff0000);
	

		g_local_handler->move_pf_window_in(hi_dev,
				s_current_reg_base,
				0xffff,
				0x1);

	}

	reg_val_w = (*(volatile unsigned *)((reg->reg_addr - s_current_reg_base)
				+ hi_dev->pci_bar1_virt));

	if (32 == bit_num_w) {
		reg_val_w = reg->value;
	} else {
		reg_val_w &= (~(((1<<bit_num_w)-1)<<bit_start_w));
		reg_val_w |= (reg->value)<<bit_start_w;
	}

	boot_trace(BOOT_INFO, "--------- write value:%x[%x]",
			reg_val_w,
			reg->reg_addr - s_current_reg_base);

	writel(reg_val_w, reg->reg_addr
			- s_current_reg_base
			+ hi_dev->pci_bar1_virt);

	while (delay_2--)
		delay();
}

static inline void read_write(struct regentry *reg,
		struct hi35xx_dev *hi_dev, unsigned int pm)
{
	unsigned int ret;
	unsigned int delay_1;

	ret = 0;
	delay_1 = reg->delay;

	if (pm) {
		if (reg->attr&W_WHETHER_PM) {
			reg_write(reg, hi_dev);
		} else if (reg->attr&R_WHETHER_PM) {
			do {
				reg_read(reg, hi_dev, &ret);
				delay();
			} while (ret);

			while (delay_1--)
				delay();
		} else {
			while (delay_1--)
				delay();
		}
	} else {
		if (reg->attr&W_WHETHER_BOOT_NORMAL) {
			reg_write(reg, hi_dev);
		} else if (reg->attr&R_WHETHER_BOOT_NORMAL) {
			do {
				reg_read(reg, hi_dev, &ret);
				delay();
			} while (ret);

			while (delay_1--)
				delay();
		} else {
			while (delay_1--)
				delay();
		}
	}
}

void pcie_init_registers(struct regentry reg_table[],
		struct hi35xx_dev *hi_dev, unsigned int pm)
{
	unsigned int i;

	printk(KERN_INFO "init_registers called!\n");

	for (i=0; ; i++) {
		boot_trace(BOOT_INFO, "indx %d ---------- addr:0x%x, value:0x%x",
				i, reg_table[i].reg_addr, reg_table[i].value);
		if ((!reg_table[i].reg_addr)&&(!reg_table[i].value)
				&&(!reg_table[i].delay)&&(!reg_table[i].attr))
			goto main_end;

		read_write(&reg_table[i], hi_dev, pm);
	}

main_end:
	delay();
}

