
#ifndef __PCIE_MCC_ARCH_HI3531_HEADER__
#define __PCIE_MCC_ARCH_HI3531_HEADER__

#define GLOBAL_SOFT_IRQ                 58

#define PCIE0_WIN0_BASE			0x30000000
#define PCIE0_WIN0_SIZE			0x100000
#define PCIE0_WIN1_BASE			0x31000000
#define PCIE0_WIN1_SIZE			0x10000
#define PCIE0_WIN2_BASE			0x32000000
#define PCIE0_WIN2_SIZE			0x100000

#define PCIE1_WIN0_BASE			0x60000000
#define PCIE1_WIN0_SIZE			0x1000000
#define PCIE1_WIN1_BASE			0x61000000
#define PCIE1_WIN1_SIZE			0x10000
#define PCIE1_WIN2_BASE			0x62000000
#define PCIE1_WIN2_SIZE			0x100000



#define SYS_CTRL_BASE			0x20050000

/*
 ************************************************
 **** Base address: PCIe configuration space ****
 */

/* Set 1 to trigger an interrupt[slave] */
#define GLB_SOFT_IRQ_CMD_REG		0xC0

/* Self-defined interrupt status[slave] */
#define HIIRQ_STATUS_REG_OFFSET		0xC4

/*
 * Host will mark this reg to tell slave
 * the right RC slave is connecting to
 */
#define DEV_CONTROLLER_REG_OFFSET	0xC8

/*
 * Host will mark this reg to tell slave
 * the right slot_index of the slave
 */
#define DEV_INDEX_REG_OFFSET		0xCC
#define SLAVE_SHAKED_BIT		0x6

/* System control register for PCIe0 */
#define PERIPHCTRL30			0xAC
/* System control register for PCIe1 */
#define PERIPHCTRL77			0x168

/* Set this bit to trigger a pcie interrupt */
#define PCIE_INT_X_BIT			(1<<5)


#endif
