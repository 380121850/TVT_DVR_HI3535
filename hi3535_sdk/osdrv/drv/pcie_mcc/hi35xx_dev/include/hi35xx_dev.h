
#ifndef __HISI35xx_PCIE_DMA_HOST_HEADER__
#define __HISI35xx_PCIE_DMA_HOST_HEADER__

#include <linux/timer.h>
#include <linux/pci.h>

#define PCI_VENDOR_HI3531 0x19e5
#define PCI_DEVICE_HI3531 0x3531

#define PCI_VENDOR_HI3532 0x19e5
#define PCI_DEVICE_HI3532 0x3532

#define PCI_VENDOR_HI3535 0x19e5
#define PCI_DEVICE_HI3535 0x3535

#define HI35xx_DEV_MODULE_NAME		"PCIe HI35XX_DEV"
#define HI35xx_DEV_VERSION		__DATE__", "__TIME__

#define MAX_PCIE_DEVICES		0x1f

#define DEVICE_UNCHECKED_FLAG		0x0
#define DEVICE_CHECKED_FLAG		0x8080
#define DEVICE_RESTART_FLAG		0x4040

/*
 * If you don't want any /proc information about PCIe MCC and DMA,
 * in your application, un-define this macro.
 */
//#define HAS_MCC_PROC_INFO		1
#define MCC_PROC_ENABLE			1

//#define MSG_HANDLE_TIMING		1

/*
 * Message received will be processed directly in irq handler if the
 * following macro is not enabled. Message received will be processed
 * in workqueue thread context if the following macro is enabled.
 */
#define ENABLE_WORKQUEUE_FOR_RECV_MSG	1

/*
 * Two methods are supported to know is there any message
 * for the given device:
 * 1. By Pcie int-x[for host]/doorbell[for slave] interrupt:
 *    when a interrupt is triggered, one or more message came.
 * 2. By a timer with a timing interval 10 mili seconds:
 *    Check the shared memory buffer every 10 ms to see is
 *    there message for me.
 *
 * If IRQ_TRIGGER is defined, int-x/doorbell interrupt is used.
 * Otherwise a timer is used.
 */
#undef MSG_IRQ_ENABLE
/* #define MSG_IRQ_ENABLE		1 */

/*
 * PCIe internal DMA supports full duplex operation,
 * processing Read and Write Transfers at the same time,
 * and in parallel with normal (non-DMA) traffic.
 * this macro enable bi-direction DMA transfer.
 */
#define DOUBLE_DMA_TRANS_LIST		1

#define HI35XX_DEBUG		4
#define HI35XX_INFO		3
#define HI35XX_ERR		2
#define HI35XX_FATAL		1
#define HI35XX_CURRENT_LEVEL	2
#define HI_TRACE(level, s, params...)	do{ \
	if(level <= HI35XX_CURRENT_LEVEL)	\
	printk("[%s, %d]: " s "\n", __func__, __LINE__, ##params);	\
}while (0)

/*
 * Offset of Key registers in Hi3535 PCIe configuration space
 */
enum pcie_config_reg_offset {
	CFG_VENDORID_REG		= 0x0,
	CFG_COMMAND_REG			= 0x04,
	CFG_CLASS_REG			= 0x08,
	CFG_BAR0_REG			= 0x10,
	CFG_BAR1_REG			= 0x14,
	CFG_BAR2_REG			= 0x18,
	CFG_BAR3_REG			= 0x1c,
	CFG_BAR4_REG			= 0x20,

	ATU_VIEW_PORT			= 0x900,
	ATU_REGION_CONTROL1		= 0x904,
	ATU_REGION_CONTROL2		= 0x908,
	ATU_BASE_LOWER			= 0x90c,
	ATU_BASE_UPPER			= 0x910,
	ATU_LIMIT			= 0x914,
	ATU_TARGET_LOWER		= 0x918,
	ATU_TARGET_UPPER		= 0x91c,

	DMA_WRITE_ENGINE_ENABLE		= 0x97c,
	DMA_WRITE_DOORBELL		= 0x980,
	DMA_READ_ENGINE_ENABLE		= 0x99c,
	DMA_READ_DOORBELL		= 0x9a0,
	DMA_WRITE_INTERRUPT_STATUS	= 0x9bc,
	DMA_WRITE_INTERRUPT_MASK	= 0x9c4,
	DMA_WRITE_INTERRUPT_CLEAR	= 0x9c8,
	DMA_READ_INTERRUPT_STATUS	= 0xa10,
	DMA_READ_INTERRUPT_MASK		= 0xa18,
	DMA_READ_INTERRUPT_CLEAR	= 0xa1c,
	DMA_CHANNEL_CONTEXT_INDEX	= 0xa6c,
	DMA_CHANNEL_CONTROL		= 0xa70,
	DMA_TRANSFER_SIZE		= 0xa78,
	DMA_SAR_LOW			= 0xa7c,
	DMA_SAR_HIGH			= 0xa80,
	DMA_DAR_LOW			= 0xa84,
	DMA_DAR_HIGH			= 0xa88,
};

#if 0
enum dma_channel_type {
	PCI_DMA_READ		= 0x0,
	PCI_DMA_WRITE		= 0x1,
};

struct pcit_dma_task {                               
	struct list_head list;                    
	unsigned int state;                       
	unsigned int dir;                        
	unsigned int src;                        
	unsigned int dest;                       
	unsigned int len;                         
	//unsigned int type;
	void *private_data;                       
	void (*finish)(struct pcit_dma_task *task);
}; 

struct pcit_dma_ptask {
	struct list_head list;
	unsigned int type;
	struct pcit_dma_task task;
};
#endif

struct pci_controller {
	/*
	 * Some archs have more than one pci controller,
	 * like Hi3531{pcie0[id:0] and pcie1[id:1]};
	 * Some archs have one only,
	 * like Hi3532{pci0[id:0]}.
	 */
	unsigned id;

	/* PCI int-x interrupt vectors */
	unsigned int int_a;
	unsigned int int_b;
	unsigned int int_c;
	unsigned int int_d;
	/* PCI MSI interrupt vector */
	unsigned int msi_irq;

	/* PCI DMA irq vector */
	unsigned int dma_local_irq;
	unsigned int dma_remote_irq;

	struct list_head list;

	/* Base address of PCI controller configuration space */
	unsigned int config_base;
	unsigned int config_virt;
	unsigned int config_size;

	/* only valid in slave */
	unsigned int win_base[6];
	unsigned int win_virt[6];
	unsigned int win_size[6];

	/*
	 * flag that marks whether this PCI controller is
	 * used or not.
	 * 0:	this controller is not used.
	 * !0:	this controller is used.
	 */
	unsigned int used_flag;

};

struct hi35xx_dev {
	unsigned int slot_index;
	unsigned int controller;

	/*
	 * In host side:
	 *  hi35xx_dev::pci_controller indicats the right PCI
	 *  controller the given hi_dev[slv] is subordinate to.
	 *  |-----------|
	 *  |	    PCI0|--------------hi_dev[m]
	 *  |		|
	 *  |	Host	|
	 *  |		|
	 *  |	    PCI1|--------------hi_dev[n]
	 *  |___________|
	 *
	 * In slave side:
	 *  |-----------|
	 *  |		|
	 *  |	Slave	|--------------hi_dev[host].[PCI0 or PCI1 or PCIx]
	 *  |		|
	 *  |___________|
	 */
	struct pci_controller pci_controller;

	/*
	 * Each remote device structure has a timer to inform the
	 * remote device that Message has been sent, plz check.
	 */
	struct timer_list timer;

	/*
	 * The irq vector used, either PCI int-x, or PCI MSI, or
	 * others like a doorbell interrupt.
	 */
	unsigned int irq;

	/*
	 * The combination of Vendor_id and Device_id
	 * This information is stored within the first 4Bytes of
	 * PCI configuration space.
	 */
	unsigned int device_id;

	unsigned int pci_bar0_virt;
	unsigned int pci_bar1_virt;
	unsigned int pci_bar2_virt;
	unsigned int pci_bar3_virt;
	unsigned int pci_bar4_virt;
	unsigned int pci_bar5_virt;
	
	unsigned int bar0;
	unsigned int bar1;
	unsigned int bar2;
	unsigned int bar3;
	unsigned int bar4;
	unsigned int bar5;

	unsigned int local_config_base_virt;

	/*
	 * A shared memory is used for the message communication
	 * between host and slave. The shared memory locates either
	 * on host DDR or slave DDR, And its base address and size
	 * are passed to system by module parameters.
	 */
	unsigned int shm_base;
	unsigned int shm_size;
	unsigned int shm_end;
	unsigned int shm_base_virt;
	unsigned int shm_end_virt;

	/*
	 * An handshake must be done between host and slave before
	 * Any communication starts. This member indicates whether
	 * this given device[hi35xx_dev] has finished handshake.
	 * 0:	No handshake is done.
	 * !0:	Handshake's done before, target device[hi35xx_dev]
	 *	is ready.
	 */
	unsigned int vdd_checked_success;
	
	struct pci_dev *pdev;

};

struct pci_operation {
	int local_devid;

	/*
	 * The right PCI controllers enabled in local side.
	 *
	 * For host: More than one PCI controller may be enabled.
	 *  Such as:
	 *  |-------------------|
	 *  |	    PCI0 enabled| <--------local_controller[0]
	 *  |			|
	 *  |	Hi3531[host]	|
	 *  |			|
	 *  |	    PCI1 enabled| <--------local_controller[1]
	 *  |___________________|
	 *
	 * For slave:Only one PCI controller will be enabled.
	 *  Such as:
	 *  |-------------------|
	 *  |			|
	 *  |	Hi3532[slave]	|
	 *  |	    PCI0 enabled| <--------local_controller[0]
	 *  |___________________|	Notes:index = {pcie0 or pcie1}
	 *
	 *		    NULL  <--------local_controller[1]
	 *
	 */
	struct pci_controller *local_controller[2];
	unsigned int local_controller_num;

	/* For host side, member host_controller is the same as
	 * member local_controller.
	 * For slave side, member host_controller stands for
	 * the right PCI controller within the host this slave
	 * is subordinate to.
	 */
	struct pci_controller *host_controller;

	unsigned int remote_device_number;

	unsigned int sysctl_reg_phys;
	unsigned int sysctl_reg_virt;
	unsigned int misc_reg_phys;
	unsigned int misc_reg_virt;

	/* Reserved for further extension */
	void * private_data;

#if 0
	unsigned int hiirq_status_phys;
	unsigned int hiirq_status_virt;
#endif

	/* MMC_ARCH_HOST or MMC_ARCH_SLAVE */
	unsigned int arch_type;

	int (*is_host)(void);
	unsigned int (*local_slot_number)(void);
	unsigned int (*pci_vendor_id)(struct hi35xx_dev *hidev);

	struct hi35xx_dev* (*slot_to_hidev)(unsigned int slot);
	int (*init_hidev)(struct hi35xx_dev * hidev);

	unsigned int (*pcie_controller)(int bar0);

	/* Read/write PCI dev config space. Valid in host side only */
	void (*pci_config_read)(struct hi35xx_dev *hidev, int offset, u32 *val);
	void (*pci_config_write)(struct hi35xx_dev *hidev, int offset, u32 val);

	/* PCI DMA IRQ operations */

	/* return 0 if failed */
	int (*dma_local_irq_num)(unsigned int controller);
	int (*has_dma)(void);
	void (*start_dma_task)(void *new_task);
	void (*stop_dma_transfer)(unsigned int dma_channel); 

	/*
	 * Return value:
	 * -1: DMA abort or some other exceptions occurred.
	 *  0: No DMA irq triggerred.
	 *  1: DMA irq clear successfully.
	 */
	int (*clear_dma_read_local_irq)(unsigned int controller);
	int (*clear_dma_write_local_irq)(unsigned int controller);

	void (*enable_dma_local_irq)(unsigned int controller);
	void (*disable_dma_local_irq)(unsigned int controller);

	/*
	 * Return value:
	 * 0: success;
	 * !0:failed;
	 */
	int (*dma_controller_init)(void);
	void (*dma_controller_exit)(void);
	int (*request_dma_resource)(irqreturn_t (*handler)(int irq, void *dev_id));
	void (*release_dma_resource)(void);

	/* PCI Message IRQ operations */
	int (*hiirq_num)(void);

	/*
	 * Return value:
	 * -1: exception.
	 *  0: No msg irq triggerred.
	 *  1: clear successfully.
	 */
	int (*clear_msg_irq)(struct hi35xx_dev *hi_dev);
	//void (*trigger_msg_irq) (struct hi35xx_dev *hi_dev);
	void (*trigger_msg_irq) (unsigned long dev);

	/*
	 * Return value:
	 * 0: success;
	 * !0:failed;
	 */
	int (*request_msg_irq)(irqreturn_t (*handler)(int irq, void *dev_id));
	void (*release_msg_irq)(void);

	void (*write_controller_num_reg)(struct hi35xx_dev *hi_dev, unsigned int val);
	unsigned int (*read_controller_num_reg)(struct hi35xx_dev *hi_dev);

	void (*write_index_reg)(struct hi35xx_dev *hi_dev, unsigned int val);
	unsigned int (*read_index_reg)(struct hi35xx_dev *hi_dev);

	int (*is_ram_address)(unsigned int address);

	/*
	 * in:	host pci controller starts dma transfer;
	 * out:	slave pci controller starts dma transfer.
	 */
	void (*move_pf_window_in)(struct hi35xx_dev *hi_dev,
			unsigned int target_addr,
			unsigned int size,
			unsigned int index);
	void (*move_pf_window_out)(struct hi35xx_dev *hi_dev,
			unsigned int target_addr,
			unsigned int size,
			unsigned int index);

	/*
	 * Normal process of handshake between host and slave:
	 *
	 * ## host_handshake_step0:
	 *		|--------->
	 * ## host_handshake_step1:
	 *		|-- wait
	 *		|-- wait	## slv_handshake_step0:
	 *		|-- wait  <----(host_handshake_step0 done) ? done : wait
	 *		|---------> 
	 *		
	 * ## host_handshake_step2:
	 *		|-- wait
	 *		|-- wait	## slv_handshake_step1:
	 *		|-- wait  <----(host_handshake_step1 done) ? done : wait
	 *		|---------> 
	 *
	 * host_slave handshake done!!
	 */
	int (*host_handshake_step0) (struct hi35xx_dev *hi_dev);
	int (*slv_handshake_step0) (void);
	int (*host_handshake_step1) (struct hi35xx_dev *hi_dev);
	int (*slv_handshake_step1) (void);
	int (*host_handshake_step2) (struct hi35xx_dev *hi_dev);
};

extern struct pci_operation *g_local_handler;
extern struct hi35xx_dev *g_hi35xx_dev_map[MAX_PCIE_DEVICES];

//void pci_config_read(struct pci_dev *pdev, int offset, u32 *addr);
//void pci_config_write(struct pci_dev *pdev, int offset, u32 value);

#endif	/* __HISI35xx_PCIE_DMA_HOST_HEADER__ */
