/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : pciv_ext.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2008/06/16
  Description   :
  History       :
  1.Date        : 2008/06/16
    Author      :
    Modification: Created file

  2.Date        : 2008/11/20
    Author      : z44949
    Modification: Modify to support VDEC

******************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/version.h>

#include "hi_errno.h"
#include "hi_debug.h"
#include "mod_ext.h"
#include "proc_ext.h"
#include "pciv.h"
#include "mkp_pciv.h"

typedef enum hiPCIV_STATE_E
{
    PCIV_STATE_STARTED  = 0,
    PCIV_STATE_STOPPING = 1,
    PCIV_STATE_STOPPED  = 2
} PCIV_STATE_E;

static PCIV_STATE_E s_enPcivState   = PCIV_STATE_STOPPED;
static atomic_t     s_stPcivUserRef = ATOMIC_INIT(0);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
    static DECLARE_MUTEX(g_PcivIoctlMutex);
#else
    static DEFINE_SEMAPHORE(g_PcivIoctlMutex);
#endif

#define PCIV_IOCTL_DWON() \
do{\
    HI_S32 s32Tmp;\
    s32Tmp = down_interruptible(&g_PcivIoctlMutex);\
    if(s32Tmp)\
    {\
        atomic_dec(&s_stPcivUserRef);\
        return s32Tmp;\
    }\
}while(0)

#define PCIV_IOCTL_UP() up(&g_PcivIoctlMutex)

static int pcivOpen(struct inode * inode, struct file * file)
{
    file->private_data = (void*)(PCIV_MAX_CHN_NUM);
    return 0;
}

static int pcivClose(struct inode *inode ,struct file *file)
{
    return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
    static int pcivIoctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#else
    static long pcivIoctl(struct file  *file, unsigned int  cmd, unsigned long arg)
#endif
{
    HI_BOOL  bDown     = HI_TRUE;
	HI_S32   s32Ret    = -ENOIOCTLCMD;
    void __user * pArg = (void __user *)arg;
    PCIV_CHN pcivChn   = (HI_U32)(file->private_data);

    /* 如果已经受到系统停止的通知,或者已经停止 */
    if (PCIV_STATE_STARTED != s_enPcivState)
    {
        return HI_ERR_PCIV_SYS_NOTREADY;
    }
    atomic_inc(&s_stPcivUserRef);

    PCIV_IOCTL_DWON();

    switch(cmd)
    {
        case PCIV_BINDCHN2FD_CTRL:
        {
            if(copy_from_user(&pcivChn, pArg, sizeof(pcivChn)) == 0)
            {
                PCIV_CHECK_CHNID(pcivChn);
                file->private_data = (void*)(pcivChn);
            }
            s32Ret = HI_SUCCESS;
            break;
        }
        case PCIV_CREATE_CTRL:
        {
            PCIV_ATTR_S stAttr;
			if(copy_from_user(&stAttr, pArg, sizeof(PCIV_ATTR_S)))
			{
				s32Ret = -EFAULT;
				break;
			}
			s32Ret =  PCIV_Create(pcivChn, &stAttr);
            break;
        }
        case PCIV_DESTROY_CTRL:
        {
			s32Ret =  PCIV_Destroy(pcivChn);
            break;
        }
        case PCIV_SETATTR_CTRL:
        {
			PCIV_ATTR_S stAttr;
			if(copy_from_user(&stAttr, pArg, sizeof(PCIV_ATTR_S)))
			{
				s32Ret = -EFAULT;
				break;
			}
    		s32Ret = PCIV_SetAttr(pcivChn, &stAttr);
			break;
        }
		case PCIV_GETATTR_CTRL:
		{
			PCIV_ATTR_S stAttr;
			s32Ret = PCIV_GetAttr(pcivChn, &stAttr);
			if(s32Ret != HI_SUCCESS)
			{
			    break;
		    }
			if(copy_to_user(pArg,&stAttr,sizeof(PCIV_ATTR_S)))
			{
				s32Ret = -EFAULT;
			}
			break;
		}
		case PCIV_START_CTRL:
		{
			s32Ret =  PCIV_Start(pcivChn);
			break;
		}
        case PCIV_STOP_CTRL:
        {
			s32Ret =  PCIV_Stop(pcivChn);
			break;
        }
		case PCIV_MALLOC_CTRL:
		{
			PCIV_IOCTL_MALLOC_S stMalloc;

			if(copy_from_user(&stMalloc, pArg, sizeof(stMalloc)))
			{
				s32Ret = -EFAULT;
				break;
			}

			s32Ret = PCIV_Malloc(stMalloc.u32Size, &stMalloc.u32PhyAddr);

			if(HI_SUCCESS == s32Ret)
			{
			    if(copy_to_user(pArg, &stMalloc, sizeof(stMalloc)))
			    {
                    s32Ret = -EFAULT;
                    (HI_VOID)PCIV_Free(stMalloc.u32PhyAddr);
                    break;
			    }
			}
			break;
		}
		case PCIV_FREE_CTRL:
		{
		    HI_U32 u32PhyAddr;

			if(copy_from_user(&u32PhyAddr, pArg, sizeof(u32PhyAddr)))
			{
				s32Ret = -EFAULT;
				break;
			}
			s32Ret =  PCIV_Free(u32PhyAddr);
			break;
		}
		case PCIV_MALLOC_CHN_BUF_CTRL:
		{
			PCIV_IOCTL_MALLOC_CHN_BUF_S stMalloc;

			if(copy_from_user(&stMalloc, pArg, sizeof(stMalloc)))
			{
				s32Ret = -EFAULT;
				break;
			}

			s32Ret = PCIV_MallocChnBuffer(stMalloc.u32ChnId, stMalloc.u32Index, stMalloc.u32Size, &stMalloc.u32PhyAddr);

			if(HI_SUCCESS == s32Ret)
			{
			    if(copy_to_user(pArg, &stMalloc, sizeof(stMalloc)))
			    {
                    s32Ret = -EFAULT;
                    (HI_VOID)PCIV_FreeChnBuffer(pcivChn, stMalloc.u32Index);
                    break;
			    }
			}
			break;
		}
		case PCIV_FREE_CHN_BUF_CTRL:
		{
			HI_U32 u32Index;

			if(copy_from_user(&u32Index, pArg, sizeof(u32Index)))
			{
				s32Ret = -EFAULT;
				break;
			}
			s32Ret =  PCIV_FreeChnBuffer(pcivChn, u32Index);
			break;
		}
		case PCIV_GETBASEWINDOW_CTRL:
		{
		    PCIV_BASEWINDOW_S stBase;

			if(copy_from_user(&stBase, pArg, sizeof(stBase)))
			{
				s32Ret = -EFAULT;
				break;
			}

            s32Ret = PCIV_DrvAdp_GetBaseWindow(&stBase);
            if(HI_SUCCESS != s32Ret) break;

            if(copy_to_user(pArg, &stBase, sizeof(stBase)))
            {
				s32Ret = -EFAULT;
            }

		    break;
		}
		case PCIV_DMATASK_CTRL:
		{
		    HI_U32 u32CpySize;
            PCIV_DMA_TASK_S  stTask;
            static PCIV_DMA_BLOCK_S stDmaBlk[PCIV_MAX_DMABLK];

			if(copy_from_user(&stTask, pArg, sizeof(PCIV_DMA_TASK_S)))
			{
				s32Ret = -EFAULT;
				break;
			}

            u32CpySize = sizeof(PCIV_DMA_BLOCK_S) * stTask.u32Count;
			if(copy_from_user(stDmaBlk, stTask.pBlock, u32CpySize))
			{
				s32Ret = -EFAULT;
				break;
			}

            PCIV_IOCTL_UP();
            bDown = HI_FALSE;

		    stTask.pBlock = stDmaBlk;
			s32Ret =  PCIV_UserDmaTask(&stTask);
		    break;
		}
		case PCIV_GETLOCALID_CTRL:
		{
		    HI_S32 s32LocalId;

		    s32LocalId = PCIV_DrvAdp_GetLocalId();

		    s32Ret = HI_SUCCESS;
            if(copy_to_user(pArg, &s32LocalId, sizeof(s32LocalId)))
            {
				s32Ret = -EFAULT;
            }
		    break;
		}
		case PCIV_ENUMCHIPID_CTRL:
		{
		    PCIV_IOCTL_ENUMCHIP_S stChip;

            s32Ret = PCIV_DrvAdp_EunmChip(stChip.s32ChipArray);
            if(HI_SUCCESS != s32Ret) break;

            if(copy_to_user(pArg, &stChip, sizeof(stChip)))
            {
				s32Ret = -EFAULT;
            }

		    break;
		}
		case PCIV_WINVBCREATE_CTRL:
		{
		    PCIV_WINVBCFG_S stConfig;

			if(copy_from_user(&stConfig, pArg, sizeof(stConfig)))
			{
				s32Ret = -EFAULT;
				break;
			}

			s32Ret = PCIV_WindowVbCreate(&stConfig);
		    break;
		}
		case PCIV_WINVBDESTROY_CTRL:
		{
			s32Ret = PCIV_WindowVbDestroy();
		    break;
		}
        case PCIV_SHOW_CTRL:
		{
			s32Ret = PCIV_Hide(pcivChn, HI_FALSE);
		    break;
		}
        case PCIV_HIDE_CTRL:
		{
			s32Ret = PCIV_Hide(pcivChn, HI_TRUE);
		    break;
		}
        case PCIV_SETVPPCFG_CTRL:
        {
            PCIV_PREPROC_CFG_S stVppCfg;
            if(copy_from_user(&stVppCfg, pArg, sizeof(stVppCfg)))
			{
				s32Ret = -EFAULT;
				break;
			}
            s32Ret = PCIV_SetPreProcCfg(pcivChn, &stVppCfg);
            break;
        }
        case PCIV_GETVPPCFG_CTRL:
        {
            PCIV_PREPROC_CFG_S stVppCfg;
			s32Ret = PCIV_GetPreProcCfg(pcivChn, &stVppCfg);
			if(s32Ret != HI_SUCCESS)
			{
			    break;
		    }
			if(copy_to_user(pArg, &stVppCfg, sizeof(stVppCfg)))
			{
				s32Ret = -EFAULT;
			}
			break;
        }

        default:
			s32Ret = -ENOIOCTLCMD;
        break;
    }

    if(bDown == HI_TRUE) PCIV_IOCTL_UP();
	atomic_dec(&s_stPcivUserRef);

    return s32Ret;
}

HI_S32 PCIV_ExtInit(void *p)
{
	/*只要不是已停止状态就不需要初始化,直接返回成功*/
    if(s_enPcivState != PCIV_STATE_STOPPED)
    {
        return HI_SUCCESS;
    }

    if(HI_SUCCESS != PCIV_Init())
	{
        HI_TRACE(HI_DBG_ERR,HI_ID_PCIV,"PcivInit failed\n");
    	return HI_FAILURE;
	}

    HI_TRACE(HI_DBG_INFO,HI_ID_PCIV,"PcivInit success\n");
    s_enPcivState = PCIV_STATE_STARTED;

	return HI_SUCCESS;
}


HI_VOID PCIV_ExtExit(HI_VOID)
{
	/*如果已经停止,则直接返回成功,否则调用exit函数*/
    if(s_enPcivState == PCIV_STATE_STOPPED)
    {
        return ;
    }
    PCIV_Exit();
    s_enPcivState = PCIV_STATE_STOPPED;
}

static HI_VOID PCIV_Notify(MOD_NOTICE_ID_E enNotice)
{
    s_enPcivState = PCIV_STATE_STOPPING;  /* 不再接受新的IOCT */
    /* 注意要唤醒所有的用户 */
    return;
}

static HI_VOID PCIV_QueryState(MOD_STATE_E *pstState)
{
    if (0 == atomic_read(&s_stPcivUserRef))
    {
        *pstState = MOD_STATE_FREE;
    }
    else
    {
        *pstState = MOD_STATE_BUSY;
    }
    return;
}

static HI_U32 PCIV_GetVerMagic(HI_VOID)
{
	return VERSION_MAGIC;
}

static UMAP_MODULE_S s_stPcivModule = {
    .pstOwner = THIS_MODULE,
    .enModId  = HI_ID_PCIV,

    .pfnInit        = PCIV_ExtInit,
    .pfnExit        = PCIV_ExtExit,
    .pfnQueryState  = PCIV_QueryState,
    .pfnNotify      = PCIV_Notify,
    .pfnVerChecker  = PCIV_GetVerMagic,
    .pData          = HI_NULL,
};

static struct file_operations pciv_fops = {
	.owner		= THIS_MODULE,
	.open		= pcivOpen,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
    .ioctl          = pcivIoctl,
#else
    .unlocked_ioctl = pcivIoctl,
#endif
	.release    = pcivClose,
};

static struct miscdevice pciv_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "pciv",
	.fops  = &pciv_fops
};

static int __init PCIV_ModInit(void)
{
    HI_S32 s32Ret;
    CMPI_PROC_ITEM_S *item;

    item = CMPI_CreateProc(PROC_ENTRY_PCIV, PCIV_ProcShow, NULL);
    if (! item)
    {
        printk("PCIV create proc error\n");
        return -1;
    }

	s32Ret  = misc_register(&pciv_dev);
	if (s32Ret)
	{
        printk("pciv register failed  error\n");
		return s32Ret;
	}

	if (CMPI_RegisterMod(&s_stPcivModule))
    {
        goto UNREGDEV;
    }

    return HI_SUCCESS;

UNREGDEV:
	misc_deregister(&pciv_dev);
    return HI_FAILURE;

}

static void __exit PCIV_ModExit(void)
{
    CMPI_UnRegisterMod(HI_ID_PCIV);
	CMPI_RemoveProc(PROC_ENTRY_PCIV);
	misc_deregister(&pciv_dev);

    return ;
}

module_init(PCIV_ModInit);
module_exit(PCIV_ModExit);

MODULE_AUTHOR(MPP_AUTHOR);
MODULE_LICENSE(MPP_LICENSE);
MODULE_VERSION(MPP_VERSION);


