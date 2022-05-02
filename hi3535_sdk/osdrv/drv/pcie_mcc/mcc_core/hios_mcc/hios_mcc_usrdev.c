#include <linux/module.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include "../include/hi_mcc_usrdev.h"
#include <linux/sched.h>

static char version[] __devinitdata = KERN_INFO "" MCC_USR_MODULE_NAME " :" MCC_USR_VERSION "\n";

extern void *vmalloc(unsigned long size);
extern void vfree(void *addr);
spinlock_t g_mcc_usr_lock;

struct hios_mem_list
{
	struct list_head head;
	void *data;
	unsigned int data_len;
};

static struct semaphore handle_sem; 
static void *s_data_to_usr = NULL;

static int hios_mcc_notifier_recv_pci(void *vdd_handle,
		void *buf, unsigned int length)
{
	hios_mcc_handle_t mcc_handle;
	hios_mcc_handle_t *_handle;
	hios_mcc_handle_opt_t opt;

	struct hios_mem_list *mem;
	void *data;
	unsigned long flags = 0;

	mcc_handle.pci_handle = (unsigned long) vdd_handle;
	hios_mcc_getopt(&mcc_handle, &opt);

	_handle = (hios_mcc_handle_t *)opt.data;

	data = kmalloc(length + sizeof(struct hios_mem_list), GFP_ATOMIC);
	if (!data) {
		hi_mcc_trace(HI_MCC_ERR, "kmalloc failed.");
		return -1;
	}
	hi_mcc_trace(HI_MCC_INFO, "nortifier_recv addr 0x%8lx len 0x%x.",
			(unsigned long)buf, length);

	mem = (struct hios_mem_list *)data;
	mem->data = data + sizeof(struct hios_mem_list);

	//printk(KERN_ERR "data[0] 0x%x, len 0x%x\n",
	//		*((unsigned int *)buf), length);
	hi_mcc_trace(HI_MCC_INFO, "memcpy to mem->data,len[0x%x].", length);
	memcpy((void *)mem->data, (void *)buf, length);

	mem->data_len = length;

	spin_lock_irqsave(&g_mcc_usr_lock, flags);
	list_add_tail(&mem->head, &_handle->mem_list);
	spin_unlock_irqrestore(&g_mcc_usr_lock, flags);

	wake_up_interruptible(&_handle->wait);

	return 0;
}

static void usrdev_setopt_recv_pci(hios_mcc_handle_t *handle)
{
	hios_mcc_handle_opt_t opt;
	opt.recvfrom_notify = &hios_mcc_notifier_recv_pci;
	opt.data = (unsigned long) handle;
	hios_mcc_setopt(handle, &opt);	
}

static void usrdev_setopt_null(hios_mcc_handle_t *handle)
{
	hios_mcc_handle_opt_t opt;
	opt.recvfrom_notify = NULL;
	opt.data = 0;
	hios_mcc_setopt(handle, &opt);	
}

static int hi_mcc_userdev_open(struct inode *inode, struct file *file)
{
	file->private_data = 0;

	return 0;
}

static void _del_mem_list(hios_mcc_handle_t *handle)
{
	struct list_head *entry , *tmp;
	struct hios_mem_list *mem;

	/* mem list empty means no data is comming */
	if (!list_empty(&handle->mem_list)) {
		list_for_each_safe(entry,tmp, &handle->mem_list) {
			mem = list_entry(entry, struct hios_mem_list, head);
			list_del(&mem->head);
			kfree(mem);
			hi_mcc_trace(HI_MCC_INFO, "handle 0x%8lx not empty",
					(unsigned long)handle);
		}
	}
}

static int hi_mcc_userdev_release(struct inode *inode, struct file *file)
{
	hios_mcc_handle_t *handle = (hios_mcc_handle_t *) file->private_data;
	if (!handle) {
		hi_mcc_trace(HI_MCC_ERR, "handle is not open.");
		return -1;
	}
	hi_mcc_trace(HI_MCC_INFO, "close 0x%8lx", (unsigned long)handle);

	if (down_interruptible(&handle_sem)) {
		hi_mcc_trace(HI_MCC_ERR, "acquire handle sem failed!");
		return -1;
	}

	usrdev_setopt_null(handle);

	/* if mem list empty means no data comming */
	if (!list_empty(&handle->mem_list)) {
		_del_mem_list(handle);
	}

	hios_mcc_close(handle);

	file->private_data = 0;

	up(&handle_sem);

	hi_mcc_trace(HI_MCC_INFO, "release success 0x%d", 0);

	return 0;
}

static int hi_mcc_userdev_read(struct file *file, char __user *buf,
		size_t count, loff_t *f_pos)
{
	hios_mcc_handle_t *handle = (hios_mcc_handle_t *)file->private_data;
	struct list_head *entry, *tmp;
	unsigned int len = 0;
	unsigned long readed = 0;
	unsigned long flags = 0;

	if (!handle) {
		hi_mcc_trace(HI_MCC_ERR, "handle is not open.");
		return -1;
	}

	hi_mcc_trace(HI_MCC_INFO, "read  empty %d handle 0x%8lx",
			list_empty(&handle->mem_list), (unsigned long)handle);

	if (down_interruptible(&handle_sem)) {
		hi_mcc_trace(HI_MCC_ERR, "acquire handle sem failed!");
		return -1;
	}

	spin_lock_irqsave(&g_mcc_usr_lock, flags);

	/* if mem list empty means no data comming*/
	if (!list_empty(&handle->mem_list)) {
		list_for_each_safe(entry, tmp, &handle->mem_list) {
			struct hios_mem_list *mem = list_entry(entry,
					struct hios_mem_list, head);

			len = mem->data_len;
			if ((len > count) || (len > MAXIMAL_MESSAGE_SIZE)) {
				hi_mcc_trace(HI_MCC_ERR, "Message len[0x%x], read len[0x%x], "
						"maximal message size[0x%x]!",
						len, count, MAXIMAL_MESSAGE_SIZE);

				goto msg_read_err1;
			}

			
			/* Because s_data_to_usr is applied with kmalloc,
			 * memset will not schedule at any circumstances.
			 */
			//memset(s_data_to_usr, 0, len);

			memcpy(s_data_to_usr, mem->data, len);

			/* Now that message had been copied to s_data_to_usr,
			 * disconnect the list node, and free its memory.
			 */
			list_del(&mem->head);

			kfree(mem);

			break;
		}

		spin_unlock_irqrestore(&g_mcc_usr_lock, flags);

		readed = copy_to_user(buf, s_data_to_usr, len);
		if (readed) {
			hi_mcc_trace(HI_MCC_ERR, "copy to usr error!");
			goto msg_read_err2;
		}

		up(&handle_sem);

		hi_mcc_trace(HI_MCC_INFO, "read success 0x%x", len);

		return len;

	} 

msg_read_err1:
	spin_unlock_irqrestore(&g_mcc_usr_lock, flags);

msg_read_err2:
	up(&handle_sem);
	return -1;
}

static int hi_mcc_userdev_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	int ret=0;
	unsigned long writed = 0;
	char *kbuf = NULL;
	/* NORMAL_MESSAGE_SIZE : 
	 * Warnning:  128 Byte[array size] at most
	 * to avoid taking too much heap memory.
	 */
	char msg_array[NORMAL_MESSAGE_SIZE] = {0};

	hios_mcc_handle_t *handle = (hios_mcc_handle_t *)file->private_data;

	if (!handle || !buf) {
		hi_mcc_trace(HI_MCC_ERR, "handle or buffer is null!");
		return -1;
	}

	if (NORMAL_MESSAGE_SIZE < count) {
		kbuf = vmalloc(count);
		if (!kbuf) {
			hi_mcc_trace(HI_MCC_ERR, "vmalloc failed.");
			return -1;
		}

	} else if (count != 0) {

		kbuf = msg_array;

	} else {
		printk(KERN_ERR "ERR, Message length is 0!!!");
		return -1;
	}

	writed = copy_from_user(kbuf, buf, count);
	if (writed) {
		hi_mcc_trace(HI_MCC_ERR, "copy from user error.");
		goto write_err;
	}

	hi_mcc_trace(HI_MCC_INFO, "mcc_handle %p.", handle);
	ret = hios_mcc_sendto_user(handle, kbuf, count, 0);
	if (ret < 0) {
		hi_mcc_trace(HI_MCC_ERR, "sendto error!");
	}

write_err:
	if (NORMAL_MESSAGE_SIZE < count) {
		vfree(kbuf);
	}

	hi_mcc_trace(HI_MCC_INFO, "send success %d.", ret);
	return ret;
}


static long hi_mcc_userdev_ioctl(struct file *file, 
		unsigned int cmd, unsigned long arg)
{
	hios_mcc_handle_t *handle;
	struct hi_mcc_handle_attr attr;
	int check;
	int local_id;
	int ret = 0;

	hi_mcc_trace(HI_MCC_DEBUG, "[cmd:%x][type:%d][num:%d][arg:%lu].", 
			cmd, _IOC_TYPE(cmd), _IOC_NR(cmd), arg);

	if (down_interruptible(&handle_sem)) {
		hi_mcc_trace(HI_MCC_ERR, "acquire handle sem failed!");
		return -1;
	}

	if (copy_from_user((void *)&attr, (void *)arg,
				sizeof(struct hi_mcc_handle_attr)))
	{
		hi_mcc_trace(HI_MCC_ERR, "Get parameter from usr space failed!");
		ret = -1;
		goto ioctl_end;
	}

	if (_IOC_TYPE(cmd) == 'M') {
		switch (_IOC_NR(cmd)) {
			case _IOC_NR(HI_MCC_IOC_ATTR_INIT):
				hi_mcc_trace(HI_MCC_DEBUG, "HI_MCC_IOC_ATTR_INIT.");
				hios_mcc_handle_attr_init(&attr);
				if (copy_to_user((void *)arg, (void *)&attr,
							sizeof(struct hi_mcc_handle_attr)))
				{
					hi_mcc_trace(HI_MCC_ERR,
							"Copy param to usr space failed.");
					ret = -1;
					goto ioctl_end;
				}
				break;

			case _IOC_NR(HI_MCC_IOC_CONNECT):
				hi_mcc_trace(HI_MCC_DEBUG, "HI_MCC_IOC_CONNECT.");
				handle = hios_mcc_open(&attr);
				if (handle) {
					INIT_LIST_HEAD(&handle->mem_list);
					init_waitqueue_head(&handle->wait);
					file->private_data = (void *)handle;
					usrdev_setopt_recv_pci(handle);
					hi_mcc_trace(HI_MCC_INFO, "open success 0x%8lx",
							(unsigned long)handle);
				} else {
					file->private_data = NULL;
					ret = -1;
					goto ioctl_end;
				}
				break;

			case _IOC_NR(HI_MCC_IOC_CHECK):
				hi_mcc_trace(HI_MCC_DEBUG, "HI_MCC_IOC_CHECK.");
				handle = (hios_mcc_handle_t *)file->private_data;
				check = hios_mcc_check_remote(attr.target_id,
						(hios_mcc_handle_t *)handle);
				ret = check;
				goto ioctl_end;
				

			case _IOC_NR(HI_MCC_IOC_GET_LOCAL_ID):
				hi_mcc_trace(HI_MCC_DEBUG, "HI_MCC_IOC_GET_LOCAL_ID.");
				handle = (hios_mcc_handle_t *)file->private_data;
				local_id = hios_mcc_getlocalid((hios_mcc_handle_t *)handle);
				ret = local_id;
				goto ioctl_end;

			case _IOC_NR(HI_MCC_IOC_GET_REMOTE_ID):
				hi_mcc_trace(HI_MCC_DEBUG, "HI_MCC_IOC_GET_REMOTE_ID.");
				handle = (hios_mcc_handle_t *)file->private_data;
				hios_mcc_getremoteids(attr.remote_id,
						(hios_mcc_handle_t *)handle);
				if (copy_to_user((void *)arg, (void *)&attr,
							sizeof(struct hi_mcc_handle_attr)))
				{
					hi_mcc_trace(HI_MCC_ERR,
							"Copy para to usr space failed.");
					ret = -1;
					goto ioctl_end;
				}
				break;

			default:
				hi_mcc_trace(HI_MCC_ERR,
						"warning not defined cmd.");
				break;
		}
	}
ioctl_end:
	up(&handle_sem);

	return ret;
}

static unsigned int hi_mcc_userdev_poll(struct file *file,
		struct poll_table_struct *table)
{
	hios_mcc_handle_t *handle = (hios_mcc_handle_t *)file->private_data;
	if (!handle) {
		hi_mcc_trace(HI_MCC_ERR, "handle is not open");
		return -1;
	}

	poll_wait(file, &handle->wait, table);

	/* if mem list empty means no data comming */
	if (!list_empty(&handle->mem_list)) {
		hi_mcc_trace(HI_MCC_INFO, "poll not empty handle 0x%8lx",
				(unsigned long)handle);
		return POLLIN | POLLRDNORM;
	}

	return 0;
}

static struct file_operations mcc_userdev_fops = {
	.owner		= THIS_MODULE,
	.open		= hi_mcc_userdev_open,
	.release	= hi_mcc_userdev_release,
	.unlocked_ioctl = hi_mcc_userdev_ioctl,
	.write		= hi_mcc_userdev_write,
	.read		= hi_mcc_userdev_read,
	.poll		= hi_mcc_userdev_poll,
};

static struct miscdevice hi_mcc_userdev = {
	.minor  = MISC_DYNAMIC_MINOR,
	.fops   = &mcc_userdev_fops,
	.name   = "mcc_userdev"
};

int __init hi_mmc_userdev_init(void)
{
	printk(version);

	sema_init(&handle_sem, 1);

	spin_lock_init(&g_mcc_usr_lock);

	/*
	 * message receive process:
	 * shared memory --> handle.mem_list.data --> s_data_to_usr
	 * --> user space. s_data_to_usr is a temporary buffer to store
	 *  message data. We add this variable to avoid system deadlock.
	 *
	 *  NOTE: user message should never be larger than 4K Bytes.
	 */
	s_data_to_usr = kmalloc(MAXIMAL_MESSAGE_SIZE, GFP_ATOMIC);
	if (NULL == s_data_to_usr) {
		hi_mcc_trace(HI_MCC_ERR, "kmalloc for s_data_to_usr failed!");
		return -1;
	}

	misc_register(&hi_mcc_userdev);		

	return 0;
}

void __exit hi_mmc_userdev_exit(void)
{

	misc_deregister(&hi_mcc_userdev);

	kfree(s_data_to_usr);
}

module_init(hi_mmc_userdev_init);
module_exit(hi_mmc_userdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chanjinn");

