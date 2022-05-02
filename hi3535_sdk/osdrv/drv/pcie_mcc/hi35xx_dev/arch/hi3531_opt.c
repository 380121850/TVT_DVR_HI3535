/*
 * Source code of this file largely depends on Hi3531
 * architecture.
 */

#if defined SLV_ARCH_HI3531
#include "hi3531_dev.h"
#elif defined SLV_ARCH_HI3532
#include "hi3532_dev.h"
#elif defined SLV_ARCH_HI3535
#include "hi3535_dev.h"
#else
#error "Error: No proper host arch selected!"
#endif

#include "../../dma_trans/dma_trans.h"

#define PCIE0_DMA_LOCAL_IRQ		77
#define PCIE1_DMA_LOCAL_IRQ		85

#define PCIE0_INT_A			73
#define PCIE0_INT_B			74
#define PCIE0_INT_C			75
#define PCIE0_INT_D			76

#define PCIE1_INT_A			81
#define PCIE1_INT_B			82
#define PCIE1_INT_C			83
#define PCIE1_INT_D			84

#define PCIE0_LOCAL_CFG			0x20800000
#define PCIE1_LOCAL_CFG			0x20810000

#define PCIE0_MEM_BASE			0x30000000
#define PCIE0_MEM_SIZE			0x10000000
#define PCIE1_MEM_BASE			0x60000000
#define PCIE1_MEM_SIZE			0x10000000

#define RAM_BASE			0x80000000
#define RAM_END				0xffffffff

/*
 ************************************************
 **** Base address: PCIe configuration space ****
 */

/* For move PCIe window */
#define ATU_BAR_NUM_BIT(x)		(x<<0x8)
#define ATU_REG_INDEX_BIT(x)		(x<<0x0)

/* This bit will be set after a dma transferred */
#define DMA_DONE_INTERRUPT_BIT		(1<<0)

/* This bit will be set abort accurred in dma transfer */
#define DMA_ABORT_INTERRUPT_BIT		(1<<16)

#define DMA_CHANNEL_DONE_BIT		(11<<5)
#define DMA_LOCAL_INTERRUPT_ENABLE_BIT	(1<<3)
#define DMA_WRITE_CHANNEL_BIT		(0<<31)
#define DMA_READ_CHANNEL_BIT		(1<<31)

#define HI3531_DEBUG            4
#define HI3531_INFO             3
#define HI3531_ERR              2
#define HI3531_FATAL            1
#define HI3531_CURRENT_LEVEL    2
#define HI3531_TRACE(level, s, params...)   do{ \
	        if(level <= HI3531_CURRENT_LEVEL) \
	        printk("[%s, %d]: " s "\n", __func__, __LINE__, ##params); \
}while (0)


static struct pci_operation *s_pcie_opt;
spinlock_t s_dma_ops_lock;

/*
 * If address is a valid RAM address, return 1;
 * else return 0.
 */
static int is_valid_ram_address(unsigned int address)
{
	if ((address >= RAM_BASE) && (address < RAM_END))
		return 1;

	return 0;
}

/*
 * Normally, this function is called for starting a dma task.
 */
static unsigned int get_pcie_controller(int bar)
{
	unsigned int controller = 0;

#ifdef __IS_PCI_HOST__
		if ((PCIE0_MEM_BASE <= bar)
			&& ((PCIE0_MEM_BASE + PCIE0_MEM_SIZE) > bar))
			return 0;	/* device <----> controller0 */
		else if ((PCIE1_MEM_BASE <= bar)
			&& ((PCIE1_MEM_BASE + PCIE1_MEM_SIZE) > bar))
			return 1;	/* device <----> controller1 */
		else
			HI3531_TRACE(HI3531_ERR, "Get pcie controller failed,"
				       "due to wrong device BAR[0x%08x]!", bar);
#else
		if (s_pcie_opt->sysctl_reg_virt)
			controller = readl(s_pcie_opt->sysctl_reg_virt
					+ DEV_CONTROLLER_REG_OFFSET);
#endif

	if ((1 != controller) && (0 != controller))
		HI3531_TRACE(HI3531_ERR, "Get pcie controller number "
				"error[0x%08x]!", controller);

	return controller;
}

static unsigned int get_local_slot_number(void)
{
	unsigned int slot = 0;

#ifdef __IS_PCI_HOST__
#else
	slot = readl(s_pcie_opt->sysctl_reg_virt + DEV_INDEX_REG_OFFSET);
	slot &= 0x1f;
	if (0 == slot)
		HI3531_TRACE(HI3531_ERR, "invalid local slot"
				" number[0x%x]!", slot);
#endif
	return slot;
}

static unsigned int pcie_vendor_device_id(struct hi35xx_dev *hi_dev)
{
	u32 vendor_device_id = 0x0;

	s_pcie_opt->pci_config_read(hi_dev,
			CFG_VENDORID_REG,
			&vendor_device_id);

	return vendor_device_id;
}

#ifdef __IS_PCI_HOST__
static int init_hidev(struct hi35xx_dev *hi_dev)
{
	s_pcie_opt->move_pf_window_in(hi_dev,
			s_pcie_opt->sysctl_reg_phys,
			0x1000,
			1);

	return 0;
}
#endif

#ifndef __IS_PCI_HOST__
static int get_hiirq_number(void)
{
	int irq = GLOBAL_SOFT_IRQ;
	return irq;
}
#endif

static int get_pcie_dma_local_irq_number(unsigned int controller)
{
	if (1 == controller)
		return PCIE1_DMA_LOCAL_IRQ;
	else if (0 == controller)
		return PCIE0_DMA_LOCAL_IRQ;
	else
		return 0;
}

/* Only slv will enable pcie dma */
static void enable_pcie_dma_local_irq(unsigned int controller)
{
	unsigned int dma_channel_status;

	dma_channel_status = readl(s_pcie_opt->local_controller[0]->config_virt
			+ DMA_CHANNEL_CONTROL);

	dma_channel_status |= DMA_CHANNEL_DONE_BIT;
	dma_channel_status |= DMA_LOCAL_INTERRUPT_ENABLE_BIT;

	writel(dma_channel_status, s_pcie_opt->local_controller[0]->config_virt
			+ DMA_CHANNEL_CONTROL);
}

static void disable_pcie_dma_local_irq(unsigned int controller)
{
	unsigned int dma_channel_status;

	dma_channel_status = readl(s_pcie_opt->local_controller[0]->config_virt
			+ DMA_CHANNEL_CONTROL);

	dma_channel_status &= (~DMA_LOCAL_INTERRUPT_ENABLE_BIT);

	writel(dma_channel_status, s_pcie_opt->local_controller[0]->config_virt
			+ DMA_CHANNEL_CONTROL);
}

static int is_dma_supported(void)
{
#ifdef __IS_PCI_HOST__
	return 0;
#else
	return 1;
#endif
}

static void __start_dma_task(void *task)
{
#ifdef __IS_PCI_HOST__
	return;
#else
	unsigned long pcie_conf_virt = 0x0;
	struct pcit_dma_task *new_task = &((struct pcit_dma_ptask *)task)->task;
	unsigned long flags = 0;

	pcie_conf_virt = s_pcie_opt->local_controller[0]->config_virt;

	spin_lock_irqsave(&s_dma_ops_lock, flags);
	if (new_task->dir == PCI_DMA_WRITE) {
		writel(0x0,		pcie_conf_virt + DMA_WRITE_INTERRUPT_MASK);
		writel(0x0,		pcie_conf_virt + DMA_CHANNEL_CONTEXT_INDEX);

		writel(0x68,		pcie_conf_virt + DMA_CHANNEL_CONTROL);
		writel(new_task->len,	pcie_conf_virt + DMA_TRANSFER_SIZE);
		writel(new_task->src,	pcie_conf_virt + DMA_SAR_LOW);
		writel(new_task->dest,	pcie_conf_virt + DMA_DAR_LOW);

		/* start DMA transfer */
		writel(0x0,		pcie_conf_virt + DMA_WRITE_DOORBELL);
	} else if (new_task->dir == PCI_DMA_READ) {
		writel(0x0,		pcie_conf_virt + DMA_READ_INTERRUPT_MASK);
		writel(0x80000000,	pcie_conf_virt + DMA_CHANNEL_CONTEXT_INDEX);

		writel(0x68,		pcie_conf_virt + DMA_CHANNEL_CONTROL);
		writel(new_task->len,	pcie_conf_virt + DMA_TRANSFER_SIZE);
		writel(new_task->src,	pcie_conf_virt + DMA_SAR_LOW);
		writel(new_task->dest,	pcie_conf_virt + DMA_DAR_LOW);

		//barrier();
		/* start DMA transfer */
		writel(0x0,		pcie_conf_virt + DMA_READ_DOORBELL);

	} else {
		HI3531_TRACE(HI3531_ERR, "Wrong dma task data![dir 0x%x]!",
				new_task->dir);
		HI3531_TRACE(HI3531_ERR, "Start_dma_task failed!");
	}
	spin_unlock_irqrestore(&s_dma_ops_lock, flags);
#endif
}

/*
 * return:
 * -1: err;
 *  0: Not DMA read irq;
 *  1: DMA done and clear successfully.
 *  2: DMA abort and clear successfully.
 */
static int clear_dma_read_local_irq(unsigned int controller)
{
	/*
	 * PCIe DMA will be started in slave side only
	 * for Hi3531, so DMA local irq will be generated
	 * in slave side only. No need to implement this
	 * function for host side.
	 */
	unsigned int read_status;
	int ret = 1;

	read_status = readl(s_pcie_opt->local_controller[0]->config_virt + DMA_READ_INTERRUPT_STATUS);
	HI3531_TRACE(HI3531_INFO, "PCIe DMA irq status: read[0x%x]", read_status);

	if (0x0 == read_status) {
		HI3531_TRACE(HI3531_INFO, "No PCIe DMA read irq triggerred!");
		return 0;
	}

	if (unlikely(DMA_ABORT_INTERRUPT_BIT & read_status)) {
		HI3531_TRACE(HI3531_ERR, "DMA read abort !!!");
		ret = 2;
	}

	if ((DMA_DONE_INTERRUPT_BIT & read_status) || (DMA_ABORT_INTERRUPT_BIT & read_status))
		writel(DMA_ABORT_INTERRUPT_BIT | DMA_DONE_INTERRUPT_BIT,
				s_pcie_opt->local_controller[0]->config_virt + DMA_READ_INTERRUPT_CLEAR);

	/* return clear done */
	return ret;
}

/*
 * return:
 * -1: err;
 *  0: Not DMA write irq;
 *  1: DMA done and clear successfully.
 *  2: DMA abort and clear successfully.
 */
static int clear_dma_write_local_irq(unsigned int controller)
{
	/*
	 * PCIe DMA will be started in slave side only
	 * for Hi3531, so DMA local irq will be generated
	 * in slave side only. No need to implement this
	 * function for host side.
	 */
	unsigned int write_status;
	int ret = 1;

	write_status = readl(s_pcie_opt->local_controller[0]->config_virt + DMA_WRITE_INTERRUPT_STATUS);
	HI3531_TRACE(HI3531_INFO, "PCIe DMA irq status: write[0x%x]", write_status);
	if (0x0 == write_status) {
		HI3531_TRACE(HI3531_INFO, "No PCIe DMA write irq triggerred!");
		return 0;
	}
	if (unlikely(DMA_ABORT_INTERRUPT_BIT & write_status)) {
		HI3531_TRACE(HI3531_ERR, "DMA write abort !!!");
		ret = 2;
	}

	if ((DMA_DONE_INTERRUPT_BIT & write_status) || (DMA_ABORT_INTERRUPT_BIT & write_status))
		writel(DMA_ABORT_INTERRUPT_BIT | DMA_DONE_INTERRUPT_BIT,
				s_pcie_opt->local_controller[0]->config_virt + DMA_WRITE_INTERRUPT_CLEAR);

	/* return clear done */
	return ret;
}

static int dma_controller_init(void)
{
	return 0;
}

static void dma_controller_exit(void)
{
}

static int request_dma_resource(irqreturn_t (*handler)(int irq, void *id))
{
#ifndef __IS_PCI_HOST__
	int ret = 0;

	/* parameter[0] means nothing here for slave */
	clear_dma_read_local_irq(0);
	clear_dma_write_local_irq(0);
	ret = request_irq(s_pcie_opt->local_controller[0]->dma_local_irq, handler,
			0, "PCIe DMA local-irq", NULL);
	if (ret) {
		HI3531_TRACE(HI3531_ERR, "request PCIe DMA irq failed!");
		return -1;
	}

	/* parameter[0] means nothing here for slave */
	enable_pcie_dma_local_irq(0);
#endif
	return 0;
}

static void release_dma_resource(void)
{
#ifndef __IS_PCI_HOST__
	free_irq(s_pcie_opt->local_controller[0]->dma_local_irq, NULL);
#endif
}

static int request_message_irq(irqreturn_t (*handler)(int irq, void *id))
{
	return 0;
}

static void release_message_irq(void)
{
}

#ifdef __IS_PCI_HOST__
static int clear_pcie_intx(struct hi35xx_dev *hi_dev)
{
	/* Only host will use this function */
	unsigned int status = 0;

	if (!hi_dev) {
		HI3531_TRACE(HI3531_ERR, "Clear PCIe intx irq err, "
				"dev[0x%x] does not exist!",
				hi_dev->slot_index);
		return -1;
	}

#ifdef SLV_ARCH_HI3531
	/* Not used */
	status = readl(hi_dev->pci_bar1_virt + PERIPHCTRL30);
	if (status & PCIE_INT_X_BIT) {
		status &= (~PCIE_INT_X_BIT);
		writel(status, hi_dev->pci_bar1_virt + PERIPHCTRL30);
	}

	status = readl(hi_dev->pci_bar1_virt + PERIPHCTRL77);
	if (status & PCIE_INT_X_BIT) {
		status &= (~PCIE_INT_X_BIT);
		writel(status, hi_dev->pci_bar1_virt + PERIPHCTRL77);
	}
#elif defined SLV_ARCH_HI3532
	/* Not used */
	status = readl(hi_dev->pci_bar1_virt + PERIPHCTRL30);
	if (status & PCIE_INT_X_BIT) {
		status &= (~PCIE_INT_X_BIT);
		writel(status, hi_dev->pci_bar1_virt + PERIPHCTRL30);
	}
#elif defined SLV_ARCH_HI3535
	/* Not used */
#endif

	return 1;
}
#endif

#ifndef __IS_PCI_HOST__
static int clear_hiirq(void)
{
	unsigned int status = 0;

	writel(0x0, s_pcie_opt->sysctl_reg_virt + GLB_SOFT_IRQ_CMD_REG);

	status = readl(s_pcie_opt->sysctl_reg_virt + HIIRQ_STATUS_REG_OFFSET);
	if (0x0 == status) {
		HI3531_TRACE(HI3531_ERR, "Hi-irq mis-triggerred!");
		return 0;
	}

	writel(0x0, s_pcie_opt->sysctl_reg_virt + HIIRQ_STATUS_REG_OFFSET);

	return 1;
}
#endif

/*
 * return:
 * -1: err;
 *  0: Not message irq;
 *  1: clear successfully.
 */
static int clear_message_irq(struct hi35xx_dev *hi_dev)
{
#ifdef __IS_PCI_HOST__
	return clear_pcie_intx(hi_dev);
#else
	return clear_hiirq();
#endif
}

#ifdef __IS_PCI_HOST__
static void hiirq_to_slave(struct hi35xx_dev *hi_dev)
{
	if (!hi_dev) {
		HI3531_TRACE(HI3531_ERR, "hiirq to slave failed, "
				"dev[0x%x] does not exist!",
				hi_dev->slot_index);
		return;
	}

	/* Nobody except me use these registers */
	writel(0x1, hi_dev->pci_bar1_virt + HIIRQ_STATUS_REG_OFFSET);
	writel(0x1, hi_dev->pci_bar1_virt + GLB_SOFT_IRQ_CMD_REG);
}
#endif

#ifndef __IS_PCI_HOST__
static void pcie_intx_irq(void)
{
	unsigned int val = 0x0;

	/* Not used */
	if (s_pcie_opt->local_controller[0]->id == 0x0) {
		val = readl(s_pcie_opt->sysctl_reg_virt + PERIPHCTRL30);
		val |= PCIE_INT_X_BIT;
		writel(val, s_pcie_opt->sysctl_reg_virt + PERIPHCTRL30);
		return;
	}

	val = readl(s_pcie_opt->sysctl_reg_virt + PERIPHCTRL77);
	val |= PCIE_INT_X_BIT;
	writel(val, s_pcie_opt->sysctl_reg_virt + PERIPHCTRL77);
}
#endif

static void trigger_message_irq(unsigned long hi_dev)
{
#ifdef __IS_PCI_HOST__
	hiirq_to_slave((struct hi35xx_dev *)hi_dev);
#else
	pcie_intx_irq();
#endif
}

static void move_pf_window_in(struct hi35xx_dev *hi_dev,
		unsigned int target_address,
		unsigned int size,
		unsigned int index)
{
#ifdef __IS_PCI_HOST__
	if (!hi_dev) {
		HI3531_TRACE(HI3531_ERR, "move pcie pf window failed,"
				" hi_dev[0x%x] is NULL!",
				hi_dev->slot_index);
		return;
	}

	s_pcie_opt->pci_config_write(hi_dev, CFG_COMMAND_REG, 0x00100147);

	/*
	 * bit 31 indicates the mapping mode: inbound(host looks at slave);
	 * bit 0 indicates the mapping viewport number;
	 */
	s_pcie_opt->pci_config_write(hi_dev, ATU_VIEW_PORT, 0x80000000 | ATU_REG_INDEX_BIT(index));

	/* No need to configure cfg_lower_base in inbound mode */
	s_pcie_opt->pci_config_write(hi_dev, ATU_BASE_LOWER, 0x0);
	s_pcie_opt->pci_config_write(hi_dev, ATU_BASE_UPPER, 0x0);
	s_pcie_opt->pci_config_write(hi_dev, ATU_LIMIT, 0xffffffff);
	s_pcie_opt->pci_config_write(hi_dev, ATU_TARGET_LOWER, target_address);
	s_pcie_opt->pci_config_write(hi_dev, ATU_TARGET_UPPER, 0x0);
	s_pcie_opt->pci_config_write(hi_dev, ATU_REGION_CONTROL1, 0x0);

	/* bit 11~9 indicates the right BAR this window bind to */
	s_pcie_opt->pci_config_write(hi_dev, ATU_REGION_CONTROL2, 0xc0000000 | ATU_BAR_NUM_BIT(index));
#else
	unsigned int local_config_virt = s_pcie_opt->local_controller[0]->config_virt;
	writel(0x00100147, local_config_virt + CFG_COMMAND_REG);

	/*
	 * bit 31 indicates the mapping mode: inbound(host looks at slave);
	 * bit 0 indicates the mapping viewport number;
	 */
	writel(0x80000000 | ATU_REG_INDEX_BIT(index), local_config_virt + ATU_VIEW_PORT);

	/* No need to configure cfg_lower_base in inbound mode */
	writel(0x0, local_config_virt + ATU_BASE_LOWER);
	writel(0x0, local_config_virt + ATU_BASE_UPPER);
	writel(0xffffffff, local_config_virt + ATU_LIMIT);
	writel(target_address, local_config_virt + ATU_TARGET_LOWER);
	writel(0x0, local_config_virt + ATU_TARGET_UPPER);
	writel(0x0, local_config_virt + ATU_REGION_CONTROL1);

	/* bit 11~9 indicates the right BAR this window bind to */
	writel(0xc0000000 | ATU_BAR_NUM_BIT(index), local_config_virt + ATU_REGION_CONTROL2);
#endif
}

/*
 * inbound: move_pf_window_out(0x2, 0x82000000, 0x800000, 0x0);
 * outbound: move_pf_window_out(0x31000000, 0x82000000, 0x800000, 0x0);
 */
static void move_pf_window_out(struct hi35xx_dev *hi_dev,
		unsigned int target_address,
		unsigned int size,
		unsigned int index)
{
#ifdef __IS_PCI_HOST__
	if (!hi_dev) {
		HI3531_TRACE(HI3531_ERR, "move pcie pf window failed,"
				" hi_dev[0x%x] is NULL!",
				hi_dev->slot_index);
		return;
	}

#else
	unsigned int local_config_virt = s_pcie_opt->local_controller[0]->config_virt;

	/*
	 * bit 31 indicates the mapping mode: outbound(slave looks at host);
	 * bit 0 indicates the mapping viewport number;
	 */
	writel(ATU_REG_INDEX_BIT(index), local_config_virt + ATU_VIEW_PORT);

	writel(s_pcie_opt->local_controller[0]->win_base[index], local_config_virt + ATU_BASE_LOWER);
	writel(0x0, local_config_virt + ATU_BASE_UPPER);
	writel(s_pcie_opt->local_controller[0]->win_base[index]
			| (s_pcie_opt->local_controller[0]->win_size[index] - 1),
			local_config_virt + ATU_LIMIT);
	writel(target_address, local_config_virt + ATU_TARGET_LOWER);
	writel(0x0, local_config_virt + ATU_TARGET_UPPER);
	writel(0x0, local_config_virt + ATU_REGION_CONTROL1);

	/* bit 11~9 indicates the right BAR this window bind to */
	writel(0xc0000000 | ATU_BAR_NUM_BIT(index), local_config_virt + ATU_REGION_CONTROL2);
#endif
}

static void clear_dma_registers(void)
{

	/* clear dma registers */
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_CHANNEL_CONTEXT_INDEX);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_SAR_HIGH);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_SAR_LOW);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_TRANSFER_SIZE);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_DAR_LOW);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_DAR_HIGH);

	/* clear not used DMA registers */
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0x9d0);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0x9d4);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0x9d8);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0x9dc);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0x9e0);

	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa8c);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa90);

	writel(0x80000000, s_pcie_opt->local_controller[0]->config_virt + DMA_CHANNEL_CONTEXT_INDEX);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_SAR_HIGH);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_SAR_LOW);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_TRANSFER_SIZE);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_DAR_LOW);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_DAR_HIGH);

	/* clear registers not used */
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa3c);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa40);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa44);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa48);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa4c);

	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa8c);
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + 0xa90);

	/* clear DMA channel controll register */
	writel(0x0, s_pcie_opt->local_controller[0]->config_virt + DMA_CHANNEL_CONTROL);

	/* enable dma write engine */
	writel(0x1, s_pcie_opt->local_controller[0]->config_virt + DMA_WRITE_ENGINE_ENABLE);

	/* enable dma read engine */
	writel(0x1, s_pcie_opt->local_controller[0]->config_virt + DMA_READ_ENGINE_ENABLE);

}

#ifdef __IS_PCI_HOST__
static int host_pcie_controller_init(void)
{
	unsigned int controller_id;
	int ret = 0;

#if (HOST_PCIE0_ENABLED == 1) && (HOST_PCIE1_ENABLED == 1)
	printk(KERN_INFO "Two PCIe controllers available!\n");
	s_pcie_opt->local_controller_num = 2;
#elif (HOST_PCIE0_ENABLED == 0) && (HOST_PCIE1_ENABLED == 0)
	HI3531_TRACE(HI3531_ERR, "No PCIe controller enabled in host!");
	s_pcie_opt->local_controller_num = 0;
	/* quit here */
	return -1;
#elif (HOST_PCIE0_ENABLED == 1)
	printk(KERN_INFO "Only PCIe0 available!\n");
	controller_id = 0;
	s_pcie_opt->local_controller_num = 1;
	goto malloc_one_pcie;
#else
	printk(KERN_INFO "Only PCIe1 available!\n");
	controller_id = 1;
	s_pcie_opt->local_controller_num = 1;
	goto malloc_one_pcie;
#endif
	s_pcie_opt->local_controller[1] = kmalloc(sizeof(struct pci_controller), GFP_KERNEL);
	if (NULL == s_pcie_opt->local_controller[1]) {
		HI3531_TRACE(HI3531_ERR, "kmalloc for pcie1 failed!");
		return -ENOMEM;
	}
	s_pcie_opt->local_controller[1]->id = 1;

	s_pcie_opt->local_controller[1]->int_a = PCIE1_INT_A;
	s_pcie_opt->local_controller[1]->int_b = PCIE1_INT_B;
	s_pcie_opt->local_controller[1]->int_c = PCIE1_INT_C;
	s_pcie_opt->local_controller[1]->int_d = PCIE1_INT_D;
	s_pcie_opt->local_controller[1]->dma_local_irq = PCIE1_DMA_LOCAL_IRQ;
	s_pcie_opt->local_controller[1]->config_base = PCIE1_LOCAL_CFG;

	s_pcie_opt->local_controller[1]->config_virt =
		(unsigned int)ioremap_nocache(s_pcie_opt->local_controller[1]->config_base, 0x1000);
	if (!s_pcie_opt->local_controller[1]->config_virt) {
		HI3531_TRACE(HI3531_ERR, "ioremap for pcie1_cfg failed!\n");
		ret = -ENOMEM;
		goto alloc_pcie_cfg_err;
	}

	clear_dma_registers();

	s_pcie_opt->local_controller[1]->used_flag = 0x1;

	controller_id = 0;

#if (HOST_PCIE0_ENABLED != HOST_PCIE1_ENABLED)
malloc_one_pcie:
#endif
	s_pcie_opt->local_controller[0] = kmalloc(sizeof(struct pci_controller), GFP_KERNEL);
	if (NULL == s_pcie_opt->local_controller[0]) {
		HI3531_TRACE(HI3531_ERR, "kmalloc for pcie%d failed!\n", controller_id);
		ret = -ENOMEM;
		goto alloc_pcie_cfg_err;
	}
	s_pcie_opt->local_controller[0]->id = controller_id;

	if (1 == controller_id) {
		s_pcie_opt->local_controller[0]->int_a = PCIE1_INT_A;
		s_pcie_opt->local_controller[0]->int_b = PCIE1_INT_B;
		s_pcie_opt->local_controller[0]->int_c = PCIE1_INT_C;
		s_pcie_opt->local_controller[0]->int_c = PCIE1_INT_D;
		s_pcie_opt->local_controller[0]->dma_local_irq = PCIE1_DMA_LOCAL_IRQ;
		s_pcie_opt->local_controller[0]->config_base = PCIE1_LOCAL_CFG;
	} else {
		s_pcie_opt->local_controller[0]->int_a = PCIE0_INT_A;
		s_pcie_opt->local_controller[0]->int_b = PCIE0_INT_B;
		s_pcie_opt->local_controller[0]->int_c = PCIE0_INT_C;
		s_pcie_opt->local_controller[0]->int_c = PCIE0_INT_D;
		s_pcie_opt->local_controller[0]->dma_local_irq = PCIE0_DMA_LOCAL_IRQ;
		s_pcie_opt->local_controller[0]->config_base = PCIE0_LOCAL_CFG;
	}

	s_pcie_opt->local_controller[0]->config_virt = (unsigned int)ioremap_nocache(s_pcie_opt->local_controller[0]->config_base, 0x1000);
	if (!s_pcie_opt->local_controller[0]->config_virt) {
		HI3531_TRACE(HI3531_ERR, "ioremap for pcie%d_cfg failed!", controller_id);
		ret = -ENOMEM;
		goto alloc_pcie_cfg_err;
	}

	s_pcie_opt->local_controller[0]->used_flag = 0x1;

	return 0;

alloc_pcie_cfg_err:
	if (s_pcie_opt->local_controller[0]) {
		if (s_pcie_opt->local_controller[0]->config_base)
			iounmap((void *)s_pcie_opt->local_controller[0]->config_base);
		kfree((void *)s_pcie_opt->local_controller[0]);
	}

	if (s_pcie_opt->local_controller[1]) {
		if (s_pcie_opt->local_controller[1]->config_base)
			iounmap((void *)s_pcie_opt->local_controller[1]->config_base);
		kfree((void *)s_pcie_opt->local_controller[1]);
	}

	return ret;
}

static void host_pcie_controller_exit(void)
{
	if (s_pcie_opt->local_controller[0]) {
		if (s_pcie_opt->local_controller[0]->config_virt)
			iounmap((void *)s_pcie_opt->local_controller[0]->config_virt);
		kfree((void *)s_pcie_opt->local_controller[0]);
	}

	if (s_pcie_opt->local_controller[1]) {
		if (s_pcie_opt->local_controller[1]->config_virt)
			iounmap((void *)s_pcie_opt->local_controller[1]->config_virt);
		kfree((void *)s_pcie_opt->local_controller[1]);
	}

}
#endif

#ifdef __IS_PCI_HOST__
static int host_handshake_step0(struct hi35xx_dev *hi_dev)
{
	unsigned int slot = 0;

	/*
	 * INDEX_REGISTER[slave:0x200500cc]:
	 *
	 *	bit[31~12]      bit[11~7]       bit[6]          bit[5~0]
	 *	shm_base_phys   reserved        slv_hand_flg    slot_index
	 *
	 * shm_base_phys: Physical address of shared memory buffer.
	 * slv_hand_flg: Flag that marks the slave side has read its
	 * slot_index: Host module initialization will enumerate all its
	 *	slave devices, and number each device with an slot index.
	 *
	 * Hand shake:
	 * 1. Host side starts and enumerates its slave devices, and send
	 *	device number to domain slot_index of this register in
	 *	slave side. Host will wait here until slv_hand_flg is
	 *	marked as 1 which indicates slave is ready.
	 * 2. Slave side mcc initialization starts, and checks whether
	 *	domain slot_index of this register was marked by host
	 *	or not. If slot_index was marked(non-zero), slave will
	 *	get its slot index and mark domain slv_hand_flg as 1,
	 *	otherwise slave device will wait here until domain
	 *	slot_index was marked.
	 * 3. Host side will alloc shared memory buffer(1MB) for each slave
	 *	device, then send base address of shared memory address to
	 *	domain shm_base_phys of this register. Shared memory buffer
	 *	is page aligned(4KB), bit[11~0] of its address will always
	 *	be zero, only bit[31~12] is valid.
	 */

	/* send shared mem buf base and slot index to slave[0x200500cc] */
	slot = hi_dev->slot_index;
	slot |= hi_dev->shm_base;
	writel(slot, hi_dev->pci_bar1_virt + DEV_INDEX_REG_OFFSET);
	return 0;
}

static int host_handshake_step1(struct hi35xx_dev *hi_dev)
{
	unsigned int val = 0;
	unsigned int check_remote_timeout = 100;

	while (1) {
		val = readl(hi_dev->pci_bar1_virt + DEV_INDEX_REG_OFFSET);
		if (val & (0x1 << SLAVE_SHAKED_BIT))
			return 0;

		if (0 == check_remote_timeout--) {
			HI3531_TRACE(HI3531_ERR, "Handshake failed[device %d]",
					hi_dev->slot_index);
			return -1;
		}

		msleep(1000);
	}
}

static int host_handshake_step2(struct hi35xx_dev *hi_dev)
{
	return 0;
}

#else
static int slv_handshake_step0(void)
{
	unsigned int val = 0;
	unsigned int check_remote_timeout = 100;
	unsigned int remote_shm_base = 0x0;

	while (1) {
		val = readl(s_pcie_opt->sysctl_reg_virt + DEV_INDEX_REG_OFFSET);
		if (val)
			break;

		HI3531_TRACE(HI3531_ERR, "Host is not ready yet!");

		if (0 == check_remote_timeout--) {
			HI3531_TRACE(HI3531_ERR, "Handshake step0 timeout!");
			return -1;
		}

		msleep(1000);
	}

	if (val & (0x1 << SLAVE_SHAKED_BIT)) {
		HI3531_TRACE(HI3531_ERR, "Already checked, or error!");
		return -1;
	}
	/* Message shared buffer should always be page align */

	/*
	 *		Host shm		slave window0
	 *
	 * [shm_base]-->######## <------------- ********* <--[dev->win0_base]
	 *		########     Mapping    *********
	 *		########                *********
	 *		######## <------------- *********
	 *
	 * hi_dev->shm_base is the physical address of shm which located in host.
	 */
	remote_shm_base = (val & 0xfffff000);
	g_hi35xx_dev_map[0]->shm_base = remote_shm_base;

	/* Get slave slot_index [0x1 ~ 0x1f] */
	s_pcie_opt->local_devid = val & 0x1f;
	if (0 == s_pcie_opt->local_devid) {
		HI3531_TRACE(HI3531_ERR, "invalid local id[0],"
				" Handshake step0 failed");
		return -1;
	}

	printk("check_remote: local slot_index:0x%x, remote_shm:0x%08x\n",
			s_pcie_opt->local_devid, remote_shm_base);

	move_pf_window_out(NULL, remote_shm_base, g_hi35xx_dev_map[0]->shm_size, 0);

	val |= (0x1 << SLAVE_SHAKED_BIT);
	writel(val, s_pcie_opt->sysctl_reg_virt + DEV_INDEX_REG_OFFSET);

	return 0;
}

static int slv_handshake_step1(void)
{
	return 0;
}

static int slv_pcie_controller_init(void)
{
	int ret = 0;
	unsigned int controller_id = 0;

	spin_lock_init(&s_dma_ops_lock);

	s_pcie_opt->local_controller[1] = NULL;
#if defined SLV_PCIE0_ENABLED
	controller_id = 0;
#elif defined SLV_PCIE1_ENABLED
	controller_id = 1;
#else
	HI3531_TRACE(HI3531_ERR, "No PCIe controller available!");
	return -1;
#endif
	s_pcie_opt->local_controller[0] = kmalloc(sizeof(struct pci_controller),GFP_KERNEL);
	if (NULL == s_pcie_opt->local_controller[0]) {
		HI3531_TRACE(HI3531_ERR, "kmalloc for pcie_controller%d failed!", controller_id);
		return -ENOMEM;
	}

	s_pcie_opt->local_controller[0]->id = controller_id;
	if (0 == controller_id) {
		s_pcie_opt->local_controller[0]->dma_local_irq = PCIE0_DMA_LOCAL_IRQ;
		s_pcie_opt->local_controller[0]->config_base = PCIE0_LOCAL_CFG;

		s_pcie_opt->local_controller[0]->win_base[0] = PCIE0_WIN0_BASE;
		s_pcie_opt->local_controller[0]->win_size[0] = PCIE0_WIN0_SIZE;
		s_pcie_opt->local_controller[0]->win_base[1] = PCIE0_WIN1_BASE;
		s_pcie_opt->local_controller[0]->win_size[1] = PCIE0_WIN1_SIZE;
		s_pcie_opt->local_controller[0]->win_base[2] = PCIE0_WIN2_BASE;
		s_pcie_opt->local_controller[0]->win_size[2] = PCIE0_WIN2_SIZE;
	} else {
		s_pcie_opt->local_controller[0]->dma_local_irq = PCIE1_DMA_LOCAL_IRQ;
		s_pcie_opt->local_controller[0]->config_base = PCIE1_LOCAL_CFG;

		s_pcie_opt->local_controller[0]->win_base[0] = PCIE1_WIN0_BASE;
		s_pcie_opt->local_controller[0]->win_size[0] = PCIE1_WIN0_SIZE;
		s_pcie_opt->local_controller[0]->win_base[1] = PCIE1_WIN1_BASE;
		s_pcie_opt->local_controller[0]->win_size[1] = PCIE1_WIN1_SIZE;
		s_pcie_opt->local_controller[0]->win_base[2] = PCIE1_WIN2_BASE;
		s_pcie_opt->local_controller[0]->win_size[2] = PCIE1_WIN2_SIZE;
	}

	s_pcie_opt->local_controller[0]->config_virt
		= (unsigned int)ioremap_nocache(s_pcie_opt->local_controller[0]->config_base,
				0x1000);
	if (!s_pcie_opt->local_controller[0]->config_virt) {
		HI3531_TRACE(HI3531_ERR, "ioremap for pcie%d_cfg failed!",
				controller_id);
		ret = -ENOMEM;
		goto alloc_config_err;
	}

	s_pcie_opt->local_controller[0]->win_virt[0]
		= (unsigned int)ioremap_nocache(s_pcie_opt->local_controller[0]->win_base[0],
				s_pcie_opt->local_controller[0]->win_size[0]);
	if (!s_pcie_opt->local_controller[0]->win_virt[0]) {
		HI3531_TRACE(HI3531_ERR, "ioremap for win0 failed!");
		ret = -ENOMEM;
		goto alloc_win0_err;
	}

	s_pcie_opt->local_controller[0]->win_virt[1]
		= (unsigned int)ioremap_nocache(s_pcie_opt->local_controller[0]->win_base[1],
				s_pcie_opt->local_controller[0]->win_size[1]);
	if (!s_pcie_opt->local_controller[0]->win_virt[1]) {
		HI3531_TRACE(HI3531_ERR, "ioremap for win1 failed!");
		ret = -ENOMEM;
		goto alloc_win1_err;
	}

	s_pcie_opt->local_controller[0]->win_virt[2]
		= (unsigned int)ioremap_nocache(s_pcie_opt->local_controller[0]->win_base[2],
				s_pcie_opt->local_controller[0]->win_size[2]);
	if (!s_pcie_opt->local_controller[0]->win_virt[2]) {
		HI3531_TRACE(HI3531_ERR, "ioremap for win2 failed!");
		ret = -ENOMEM;
		goto alloc_win2_err;
	}

	/* The initial value of these registers are not zero, clear them */
	clear_dma_registers();

	s_pcie_opt->local_controller[0]->used_flag = 0x1;

	return 0;

alloc_win2_err:
	iounmap((void *)s_pcie_opt->local_controller[0]->win_virt[1]);
alloc_win1_err:
	iounmap((void *)s_pcie_opt->local_controller[0]->win_virt[0]);
alloc_win0_err:
	iounmap((void *)s_pcie_opt->local_controller[0]->config_virt);
alloc_config_err:
	kfree(s_pcie_opt->local_controller[0]);

	return -1;
}

static void slv_pcie_controller_exit(void)
{
	if (s_pcie_opt->local_controller[0]) {
		if (s_pcie_opt->local_controller[0]->win_virt[2])
			iounmap((void *)s_pcie_opt->local_controller[0]->win_virt[2]);
		if (s_pcie_opt->local_controller[0]->win_virt[1])
			iounmap((void *)s_pcie_opt->local_controller[0]->win_virt[1]);
		if (s_pcie_opt->local_controller[0]->win_virt[0])
			iounmap((void *)s_pcie_opt->local_controller[0]->win_virt[0]);
		if (s_pcie_opt->local_controller[0]->config_virt)
			iounmap((void *)s_pcie_opt->local_controller[0]->config_virt);

		kfree(s_pcie_opt->local_controller[0]);
	}
}
#endif

int pci_arch_init(struct pci_operation *handler)
{
	int ret = 0;

	s_pcie_opt = handler;

	s_pcie_opt->sysctl_reg_phys		= SYS_CTRL_BASE;

	s_pcie_opt->move_pf_window_in		= move_pf_window_in;
	s_pcie_opt->move_pf_window_out		= move_pf_window_out;

	s_pcie_opt->local_slot_number		= get_local_slot_number;
	s_pcie_opt->pci_vendor_id		= pcie_vendor_device_id;
	s_pcie_opt->pcie_controller		= get_pcie_controller;

	s_pcie_opt->clear_msg_irq		= clear_message_irq;
	s_pcie_opt->trigger_msg_irq		= trigger_message_irq;
	s_pcie_opt->request_msg_irq		= request_message_irq;
	s_pcie_opt->release_msg_irq		= release_message_irq;

	s_pcie_opt->is_ram_address		= is_valid_ram_address;
	s_pcie_opt->dma_controller_init		= dma_controller_init;
	s_pcie_opt->dma_controller_exit		= dma_controller_exit;
	s_pcie_opt->request_dma_resource	= request_dma_resource;
	s_pcie_opt->release_dma_resource	= release_dma_resource;
	s_pcie_opt->dma_local_irq_num		= get_pcie_dma_local_irq_number;
	s_pcie_opt->has_dma			= is_dma_supported;
	s_pcie_opt->start_dma_task		= __start_dma_task;
	s_pcie_opt->clear_dma_write_local_irq	= clear_dma_write_local_irq;
	s_pcie_opt->clear_dma_read_local_irq	= clear_dma_read_local_irq;
	s_pcie_opt->enable_dma_local_irq	= enable_pcie_dma_local_irq;
	s_pcie_opt->disable_dma_local_irq	= disable_pcie_dma_local_irq;

#if defined __IS_PCI_HOST__
	s_pcie_opt->init_hidev			= init_hidev; /* host only */

	s_pcie_opt->host_handshake_step0	= host_handshake_step0;
	s_pcie_opt->host_handshake_step1	= host_handshake_step1;
	s_pcie_opt->host_handshake_step2	= host_handshake_step2;

	if (host_pcie_controller_init()) {
		HI3531_TRACE(HI3531_ERR, "Host side PCIe controller init failed!");
		ret = -1;
	}
#else
	s_pcie_opt->hiirq_num			= get_hiirq_number; /* slave only */

	s_pcie_opt->slv_handshake_step0		= slv_handshake_step0;
	s_pcie_opt->slv_handshake_step1		= slv_handshake_step1;

	if (slv_pcie_controller_init()) {
		HI3531_TRACE(HI3531_ERR, "Slave side PCIe controller init failed!");
		return -1;
	}

	s_pcie_opt->sysctl_reg_virt = (unsigned int)ioremap_nocache(s_pcie_opt->sysctl_reg_phys, 0x1000);
	if (!s_pcie_opt->sysctl_reg_virt) {
		HI3531_TRACE(HI3531_ERR, "ioremap for sysctl_reg failed!");
		ret = -ENOMEM;
	}
#endif

	/* return success */
	return ret;
}

void pci_arch_exit(void)
{
#ifdef __IS_PCI_HOST__
	host_pcie_controller_exit();
#else
	slv_pcie_controller_exit();

	if (s_pcie_opt->sysctl_reg_virt)
		iounmap((volatile void *)s_pcie_opt->sysctl_reg_virt);
#endif
}
