
#ifndef __PCIE_MCC_DMA_TRANSFER_HEADER__
#define __PCIE_MCC_DMA_TRANSFER_HEADER__

#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/semaphore.h>

#define MAX_PCIE_DEV_NUM		0x1f

#define PCIE_DMA_MODULE_NAME		"PCIe DMA Trans"
#define PCIE_DMA_VERSION		__DATE__", "__TIME__

#define DMA_ADDR_ALIGN_MASK		((1<<2)-1)

#define DMA_TIMEOUT_JIFFIES	10
#define DMA_TASK_NR		128

/*
 * There are two DMA task states:
 * 1. Task created state: from the time when user called
 *   pcit_create_task, to the time when the function
 *   pcit_create_task returns.
 * 2. Task transfer finish: since a dma task was executed,
 *   to the time that the DMA data has been transferred.
 */

#define PCIT_DEBUG  4
#define PCIT_INFO   3
#define PCIT_ERR    2
#define PCIT_FATAL  1
#define PCIT_CURR_LEVEL 2
#define PCIT_PRINT(level, s, params...)   do{ \
	if(level <= PCIT_CURR_LEVEL)  \
	printk("[%s, %d]: " s "\n", __func__, __LINE__, ##params); \
}while(0)

enum dma_channel_type {
	PCI_DMA_READ            = 0x0,
	PCI_DMA_WRITE           = 0x1,
};

struct pcit_dma_task {
	struct list_head list;
	unsigned int state;
	unsigned int dir;
	unsigned int src;
	unsigned int dest;
	unsigned int len;
	void *private_data;
	void (*finish)(struct pcit_dma_task *task);
};

struct pcit_dma_ptask {
	struct list_head list;
	unsigned int type;
	struct pcit_dma_task task;
};

enum dma_task_status {
	DMA_TASK_TODO		= 0,
	DMA_TASK_CREATED	= 1,
	DMA_TASK_FINISHED	= 2,
	DMA_TASK_EMPTY		= 0xffffffff,
};

enum dma_trans_status {
	UNKNOW_EXCEPTION	= -1,
	NO_TRANSFER		= 0,
	NORMAL_DONE		= 1,
	TRANSFER_ABORT		= 2,
};

enum dma_channel_state {
	CHANNEL_ERROR		= -1,
	CHANNEL_IDLE		= 0,
	CHANNEL_BUSY		= 1,
};

enum channel_transfer_object {
	__VDATA			= 0,
	__CMESSAGE		= 1,
	__NOBODY		= 2,
};

struct dma_object {
	struct list_head busy_list_head;
	struct list_head free_list_head;

	struct pcit_dma_ptask task_array[DMA_TASK_NR];

	void *reserved;
};

#define MSG_DMA_ENABLE
struct dma_channel {
	unsigned int type;

	spinlock_t mlock;
	struct semaphore sem;

#ifdef MSG_DMA_ENABLE
	struct dma_object msg;
#endif
	struct dma_object data;

	unsigned int error;

	int state;
	int owner;

	void *pri;
};

#define DMA_WRITE_ENABLE
#define DMA_READ_ENABLE
struct dma_control {
#ifdef DMA_WRITE_ENABLE
	struct dma_channel write;
#endif

#ifdef DMA_READ_ENABLE
	struct dma_channel read;
#endif

};


struct pcit_dev_cfg {                                                  
        unsigned int slot;                         
        unsigned short vendor_id;                  
        unsigned short device_id;               
                                                
        unsigned long np_phys_addr;             
	unsigned long np_size;                      
                                                
        unsigned long pf_phys_addr;             
	unsigned long pf_size;                         
                                                   
        unsigned long cfg_phys_addr;               
	unsigned long cfg_size;                        
};                                                 
 
struct pcit_bus_dev {
        unsigned int bus_nr; /* input argument */  
        struct pcit_dev_cfg devs[MAX_PCIE_DEV_NUM];
};        

struct pcit_event {
        unsigned int event_mask;   
        unsigned long long pts;    
};    

struct pcit_dma_req {
	unsigned int bus;
	unsigned int slot_index;
	unsigned long src;
	unsigned long dest;
	unsigned int len;
};

#define HI_IOC_PCIT_BASE		'H'

#define HI_IOC_PCIT_INQUIRE		_IOR(HI_IOC_PCIT_BASE, 1, struct pcit_bus_dev)

#define HI_IOC_PCIT_DMARD		_IOW(HI_IOC_PCIT_BASE, 2, struct pcit_dma_req)

#define HI_IOC_PCIT_DMAWR		_IOW(HI_IOC_PCIT_BASE, 3, struct pcit_dma_req)

#define HI_IOC_PCIT_BINDDEV		_IOW(HI_IOC_PCIT_BASE, 4, int)

#define HI_IOC_PCIT_DOORBELL		_IOW(HI_IOC_PCIT_BASE, 5, int)

#define HI_IOC_PCIT_SUBSCRIBE		_IOW(HI_IOC_PCIT_BASE, 6, int)

#define HI_IOC_PCIT_UNSUBSCRIBE		_IOW(HI_IOC_PCIT_BASE, 7, int)

#define HI_IOC_PCIT_LISTEN		_IOR(HI_IOC_PCIT_BASE, 8, struct pcit_event)

#define TIMING_DEBUG 0

int pcit_create_task(struct pcit_dma_task *task);
struct pcit_dma_task * __pcit_create_task(struct pcit_dma_task *task, int type);
unsigned int get_pf_window_base(int slot);


/*
 * ************************************************************
 * For PCI MCC TEST DEMO
 * ***********************************************************
 */
//#define HISI_DMA_TEST_DEMO
#ifdef HISI_DMA_TEST_DEMO

struct hi_mcc_handle_attr {
	int target_id;
	int port;
	int priority;
	int remote_id[MAX_PCIE_DEV_NUM];
};

#define HI_IOC_MCC_BASE			'M'

#define HI_MCC_IOC_CONNECT		_IOW(HI_IOC_MCC_BASE, 1, struct hi_mcc_handle_attr)

#define HI_MCC_IOC_CHECK		_IOW(HI_IOC_MCC_BASE, 2, struct hi_mcc_handle_attr)

#define HI_MCC_IOC_DISCONNECT		_IOW(HI_IOC_MCC_BASE, 3, unsigned long)

#define HI_MCC_IOC_GET_LOCAL_ID		_IOW(HI_IOC_MCC_BASE, 4, struct hi_mcc_handle_attr)

#define HI_MCC_IOC_GET_REMOTE_ID	_IOW(HI_IOC_MCC_BASE, 5, struct hi_mcc_handle_attr)

#define HI_MCC_IOC_ATTR_INIT		_IOW(HI_IOC_MCC_BASE, 6, struct hi_mcc_handle_attr)

#define HI_MCC_IOC_SETOPTION		_IOW(HI_IOC_MCC_BASE, 3, unsigned int)

#endif /* HISI_DMA_TEST_DEMO */

#endif /* __PCIE_MCC_DMA_TRANSFER_HEADER__ */
