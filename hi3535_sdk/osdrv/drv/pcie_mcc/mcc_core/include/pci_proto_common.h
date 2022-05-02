#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>

#ifndef MCC_PROTOCAL_COMMON_HEADER__
#define MCC_PROTOCAL_COMMON_HEADER__

#define MCC_DRV_MODULE_NAME		"MCC_PROTOCOL"
#define MCC_DRV_VERSION			__DATE__", "__TIME__

#define MAX_DEV_NUMBER			0x1f
#define MAX_MSG_PORTS			0x400

#define MSG_DEV_BITS			0x3F
#define MSG_PORT_BITS			0x3FF
#define MSG_PORT_NEEDBITS		10

#define HANDLE_TABLE_SIZE		(MAX_DEV_NUMBER * MAX_MSG_PORTS)

#define DEV_SHM_SIZE			768 * 1024
#define HISI_SHARED_RECVMEM_SIZE        (384 * 1024)
#define HISI_SHARED_SENDMEM_SIZE        (384 * 1024)
#define HISI_SHM_IRQSEND_SIZE		(128 * 1024)
#define HISI_SHM_IRQRECV_SIZE		(128 * 1024)

#define NORMAL_DATA			(1 << 0)
#define PRIORITY_DATA			(1 << 1)

#define HISI_MEM_DATA			(0)
#define HISI_INITIAL_MEM_DATA		(1)

#define _HEAD_SIZE (sizeof(struct hisi_pci_transfer_head))
#define _MEM_ALIAGN_CHECK (_MEM_ALIAGN - _HEAD_SIZE)

#define _MEM_ALIAGN                     (32)    /* 32bytes aliagn */
#define _MEM_ALIAGN_VERS                (_MEM_ALIAGN - 1)

#define _FIX_ALIAGN_DATA                (0xC3)  /* 11000011b */

#define _HEAD_MAGIC_INITIAL		(0x333)	/* 1100110011b */
#define _HEAD_MAGIC_JUMP		(0x249)	/* 1001001001b */
#define _HEAD_MAGIC_PRIO		(0X36D)	/* 1101101101b */
#define _HEAD_CHECK_FINISHED		(0xA95)	/* 101010010101b */

#define MCC_DRV_TIMING_DEBUG		0
//#define DMA_MESSAGE_ENABLE		1
//#define MESSAGE_IRQ_ENABLE

#define DMA_ASYNC_WAIT			1

#define MCC_DEBUG	4
#define MCC_INFO	3
#define MCC_ERR		2
#define MCC_FATAL	1
#define MCC_DBG_LEVEL	2
#define mcc_trace(level, s, params...) do{ if(level <= MCC_DBG_LEVEL)\
	printk(KERN_INFO "[%s, %d]: " s "\n", __FUNCTION__, __LINE__, ##params);\
}while(0)

#if 0
struct hisi_pcidev_map {
	struct {
		unsigned long base_phy;
		unsigned long base_virt;
		void *dev;
	} map[MAX_DEV_NUMBER];

	unsigned int num;
	struct timer_list map_timer;	
};
#endif

enum message_type {
	PRE_VIEW_MSG	= 0,
	CONTROL_MSG	= 1,
};

struct pci_vdd_info {
	unsigned long version;
	unsigned long id;
	unsigned long bus_slot;
	unsigned long mapped[MAX_DEV_NUMBER];
};

typedef struct pci_vdd_info hisi_pci_vdd_info_t;
extern hisi_pci_vdd_info_t hisi_pci_vdd_info;
extern struct hisi_transfer_handle *hisi_handle_table[HANDLE_TABLE_SIZE];

extern unsigned int shm_size;
extern unsigned int shm_phys_addr;

extern spinlock_t g_mcc_list_lock;

struct hisi_transfer_handle {
	unsigned int target_id;

	unsigned int port;
	unsigned int priority;

	int (*vdd_notifier_recvfrom)(void *handle, void *buf, unsigned int len);

	unsigned long data;
};

struct hisi_memory_info {
	volatile unsigned int base_addr;
	volatile unsigned int end_addr;
	volatile unsigned int buf_len;
	volatile unsigned int rp_addr;
	volatile unsigned int wp_addr;

	struct semaphore sem;
};

/*
 * The layout of shared memory:
 * Host's view
 *  /----------recv section---------\ /---------send section----------\
 *  /---irq----\ /------thread------\ /---irq----\ /------thread------\
 *  ############ #################### ############ ####################
 *  \---irq----/ \------thread------/ \---irq----/ \------thread------/
 *  \----------send section---------/ \---------recv section----------/
 * Slave's view
 */
struct hisi_shared_memory_info{
	struct hisi_memory_info irqmem;
	struct hisi_memory_info threadmem;
};

struct hisi_map_shared_memory_info {
	struct hisi_shared_memory_info recv;
	struct hisi_shared_memory_info send;
};

#if 0
struct hisi_map_shared_memory_info_slave{
	/* For slave, send section in front */
	struct hisi_shared_memory_info send;
	struct hisi_shared_memory_info recv;
};

#ifdef DMA_MESSAGE_ENABLE
#define DMA_EXCHG_BUF_SIZE	(256 * 1024)
#define DMA_EXCHG_READ		0
#define DMA_EXCHG_WRITE		1

struct dma_exchange_buf {
	spinlock_t mlock;
	struct semaphore sem;
	int type;
	unsigned int rp_addr;
	unsigned int wp_addr;
	unsigned int rp_addr_virt;
	unsigned int wp_addr_virt;
	unsigned int base;
	unsigned int size;
	unsigned int base_virt;
	unsigned int data_base;
	unsigned int data_end;
	unsigned int data_base_virt;
	unsigned int data_end_virt;
	wait_queue_head_t wait;
};

struct hisi_dma_exchange_buffer {
	struct dma_exchange_buf read_buf;
	struct dma_exchange_buf write_buf;
};
#endif
#endif

struct hisi_pci_transfer_head {
	volatile unsigned int target_id:6;
	volatile unsigned int src:6;
	volatile unsigned int port:10;
	volatile unsigned int magic:10;
	volatile unsigned int length:20;
	volatile unsigned int check:12;
};

struct hisi_message {
	struct hi35xx_dev *target;
	const void *src_buf;
	unsigned int len;
	struct hisi_pci_transfer_head *head;
	struct work_struct work;
	void *pri;
};

#define MAX_PCIV_MSG_WAIT_NO		0x128
#define MAX_PCIV_MSG_SIZE		0x64
struct pciv_message {
	struct list_head list;
	struct hi35xx_dev *target;
	char msg[MAX_PCIV_MSG_SIZE];
	unsigned int len;
	struct hisi_pci_transfer_head head;
	void *pri;
	unsigned int msg_flag;
	unsigned int time_start;
	int state;
};

typedef int (*vdd_notifier_recvfrom)(void *handle, 
		void *buf, unsigned int length);
struct pci_vdd_opt {
	vdd_notifier_recvfrom  recvfrom_notify;
	unsigned long data;
};

typedef struct pci_vdd_opt pci_vdd_opt_t;

/* Internal interfaces */
extern int host_send_msg(struct hisi_transfer_handle *handle, const void *buf, unsigned int len,int flag);
extern int slave_send_msg(struct hisi_transfer_handle *handle, const void *buf, unsigned int len,int flag);
extern int slave_check_host(struct hi35xx_dev *hi_dev);
extern int host_check_slv(struct hi35xx_dev *hi_dev);

/* External interfaces */
extern int pci_vdd_remoteids(int ids[]);
extern int pci_vdd_check_remote(int remote_id);
extern int pci_vdd_localid(void);
extern void *pci_vdd_open(unsigned int target_id, unsigned int port, unsigned int priority);
extern int pci_vdd_getopt(void *handle, pci_vdd_opt_t *opt);
extern int pci_vdd_setopt(void *handle, pci_vdd_opt_t *opt);
extern int pci_vdd_sendto(void *handle, const void *buf, unsigned int len, int flag);
extern void pci_vdd_close(void *handle);

#endif /* MCC_PROTOCAL_COMMON_HEADER__ */

