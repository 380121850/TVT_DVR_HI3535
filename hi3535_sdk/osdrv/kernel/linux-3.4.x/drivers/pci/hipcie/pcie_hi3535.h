#ifndef __HISI_PCIE_H__
#define __HISI_PCIE_H__

#define PCIE_SYS_BASE_PHYS	0x20120000
#define PCIE0_BASE_ADDR_PHYS	0x30000000
#define PCIE0_MEMIO_BASE	0x40000000
#define PCIE_BASE_ADDR_SIZE	0x10000000
#define DBI_BASE_ADDR_0         0x20800000

#define PERI_CRG_BASE		0x20030000

#define PERI_CRG44		0xB0
#define PCIE_BUS_CKEN		4
#define PCIE_RX0_CKEN		5
#define PCIE_AUX_CKEN		7
#define PCIE_CKO_ALIVE_CKEN	8
#define PCIE_MPLL_DWORD_CKEN	9
#define PCIE_REFCLK_CKEN	10
#define PCIE_BUS_SRST_REQ	12

#define PCIE_SYS_CTRL0		0xF0
#define PCIE_DEVICE_TYPE	28
#define PCIE_WM_EP		0x0
#define PCIE_WM_LEGACY		0x1
#define PCIE_WM_RC		0x4

#define PCIE_SYS_CTRL7		0x10C
#define PCIE0_APP_LTSSM_ENBALE	11

#define PCIE_SYS_CTRL13		0x124
#define PCIE_CFG_REF_USE_PAD	29

#define PCIE_SYS_CTRL15         0x12C
#define PCIE_XMLH_LINK_UP	15
#define PCIE_RDLH_LINK_UP	5

#define IRQ_BASE		32

#define PCIE0_IRQ_INTA          (IRQ_BASE + 48)
#define PCIE0_IRQ_INTB          (IRQ_BASE + 49)
#define PCIE0_IRQ_INTC          (IRQ_BASE + 50)
#define PCIE0_IRQ_INTD          (IRQ_BASE + 51)
#define PCIE0_IRQ_EDMA		(IRQ_BASE + 52)
#define PCIE0_IRQ_MSI		(IRQ_BASE + 53)
#define PCIE0_IRQ_LINK_DOWN	(IRQ_BASE + 54)

#define PCIE_INTA_PIN		1
#define PCIE_INTB_PIN		2
#define PCIE_INTC_PIN		3
#define PCIE_INTD_PIN		4

#endif
