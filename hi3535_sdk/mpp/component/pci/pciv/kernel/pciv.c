/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : pciv.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2009/07/16
  Description   :
  History       :
  1.Date        : 2009/07/16
    Author      : Z44949
    Modification: Created file
  2.Date        : 2009/09/12
    Author      : P00123320
    Modification: Porting to Hi3520, fix some bugs and modify some func
  3.Date        : 2009/10/10
    Author      : P00123320
    Modification: Add function for hide/show pciv chn
  4.Date        : 2009/10/20
    Author      : P00123320
    Modification: When destroy pciv chn,first wait finish all transfers
  5.Date        : 2009/11/2
    Author      : P00123320
    Modification: Modify function PCIV_Create().
  6.Date        : 2010/2/11
    Author      : P00123320
    Modification: Some code in func:PcivCheckAttr remove to pciv_firmware
  7.Date        : 2010/2/24
    Author      : P00123320
    Modification: Add interfaces about set/get pre-process cfg
  8.Date        : 2010/5/29
    Author      : P00123320
    Modification: Modify struct PCIV_NOTIFY_PICEND_S, transfer enFiled additionaly.
******************************************************************************/
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "pciv.h"
#include "pciv_fmwext.h"

#define PCIV_MAX_DMATASK 32

typedef struct hiPCIV_CHANNEL_S
{
    volatile HI_BOOL bCreate;   /* 是否已创建 */
    volatile HI_BOOL bStart;    /* 是否启动传输 */
    volatile HI_BOOL bConfig;   /* 是否已配置 */
    volatile HI_BOOL bHide;     /* 是否隐藏 */

    volatile HI_U32  u32GetCnt;   /* 记录读stRecycleCb 或 stPicCb的次数 */
    volatile HI_U32  u32SendCnt;  /* 记录写stRecycleCb 或 stPicCb的次数 */
    volatile HI_U32  u32RespCnt;  /* 记录PCI完成中断响应的次数 */
    volatile HI_U32  u32LostCnt;  /* 记录源图像因无空闲Buffer而丢失图像的次数 */
    volatile HI_U32  u32NotifyCnt;  /* 被通知的次数(收到ReadDone或者WriteDone) */

    PCIV_ATTR_S      stPcivAttr; /* 记录目标图象信息以及对端设备信息 */
    volatile HI_BOOL bBufFree[PCIV_MAX_BUF_NUM];  /* Used by sender. */
    volatile HI_U32  u32BufUseCnt[PCIV_MAX_BUF_NUM];  /* Used by sender. */
    volatile HI_BOOL bCanRecv;    /* 两片间内存信息同步标志，用于接收端 */

    struct semaphore pcivMutex;
} PCIV_CHANNEL_S;

typedef struct hiPCIV_USERDMADONE_S
{
    struct list_head list;

    wait_queue_head_t stwqDmaDone;
    HI_BOOL           bDmaDone;
} PCIV_USERDMANODE_S;

static PCIV_CHANNEL_S     g_stPcivChn[PCIV_MAX_CHN_NUM];
static PCIV_USERDMANODE_S g_stUserDmaPool[PCIV_MAX_DMATASK];
static struct list_head   g_listHeadUserDma;

extern HI_U32 g_u64RdoneTime[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u32RdoneGap[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u32MaxRdoneGap[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u32MinRdoneGap[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u64WdoneTime[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u32WdoneGap[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u32MaxWdoneGap[PCIV_MAX_CHN_NUM];
extern HI_U32 g_u32MinWdoneGap[PCIV_MAX_CHN_NUM];

static spinlock_t        g_PcivLock;
#define PCIV_SPIN_LOCK   spin_lock_irqsave(&g_PcivLock,flags)
#define PCIV_SPIN_UNLOCK spin_unlock_irqrestore(&g_PcivLock,flags)

#define PCIV_MUTEX_DOWN(sema)\
    do {\
        if (down_interruptible(&(sema)))\
            return -ERESTARTSYS;\
    } while(0)

#define PCIV_MUTEX_UP(sema) up(&(sema))

HI_S32 PcivCheckAttr(PCIV_ATTR_S *pAttr)
{
    HI_S32 i;

    /* 检查缓存数目是否合理 */
    if((pAttr->u32Count < 2) || (pAttr->u32Count > PCIV_MAX_BUF_NUM))
    {
        PCIV_TRACE(HI_DBG_ERR, "Buffer count(%d) not invalid,should in [2,%d]\n", pAttr->u32Count, PCIV_MAX_BUF_NUM);
        return HI_ERR_PCIV_ILLEGAL_PARAM;
    }

    /* 检查物理地址 */
    for(i=0; i<pAttr->u32Count; i++)
    {
        if(!pAttr->u32PhyAddr[i])
        {
            PCIV_TRACE(HI_DBG_ERR, "pAttr->u32PhyAddr[%d]:0x%x invalid\n", i, pAttr->u32PhyAddr[i]);
            return HI_ERR_PCIV_ILLEGAL_PARAM;
        }
    }

    /* 检查远端设备是否有效 */
    if(  (pAttr->stRemoteObj.s32ChipId < 0)
       ||(pAttr->stRemoteObj.s32ChipId > PCIV_MAX_CHIPNUM)
       ||(pAttr->stRemoteObj.pcivChn < 0)
       ||(pAttr->stRemoteObj.pcivChn >= PCIV_MAX_CHN_NUM)
       )
     {
        PCIV_TRACE(HI_DBG_ERR, "Invalid remote object(%d,%d)\n",
                   pAttr->stRemoteObj.s32ChipId, pAttr->stRemoteObj.pcivChn);
        return HI_ERR_PCIV_ILLEGAL_PARAM;
     }

     return HI_SUCCESS;
}

HI_S32 PCIV_Create(PCIV_CHN pcivChn, PCIV_ATTR_S *pAttr)
{
    PCIV_CHANNEL_S *pChn = NULL;
    HI_S32          s32Ret, i;
    HI_S32          s32LocalId = 0;
    unsigned long   flags;

    PCIV_CHECK_CHNID(pcivChn);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);
    
    PCIV_SPIN_LOCK;
    pChn = &g_stPcivChn[pcivChn];
    if(HI_TRUE == pChn->bCreate)
    {
        PCIV_TRACE(HI_DBG_ERR, "Channel %d has been created\n", pcivChn);
        PCIV_SPIN_UNLOCK;
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_EXIST;
    }

    if(HI_SUCCESS != PcivCheckAttr(pAttr))
    {
        PCIV_SPIN_UNLOCK;
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_ILLEGAL_PARAM;
    }
    
    s32LocalId = PCIV_DrvAdp_GetLocalId();
    PCIV_SPIN_UNLOCK;

    s32Ret = PCIV_FirmWareCreate(pcivChn, pAttr, s32LocalId);
    if(HI_SUCCESS != s32Ret)
    {
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return s32Ret;
    }
    
    PCIV_SPIN_LOCK;
    pChn->bStart   = HI_FALSE;
    pChn->bConfig  = HI_TRUE;
    pChn->bCreate  = HI_TRUE;
    pChn->bCanRecv = HI_FALSE;
    pChn->bHide    = HI_FALSE;   /* reset to show */
    pChn->u32GetCnt = pChn->u32SendCnt = pChn->u32RespCnt = pChn->u32LostCnt = pChn->u32NotifyCnt = 0;

    for (i = 0; i < PCIV_MAX_BUF_NUM; i++)
    {
        pChn->bBufFree[i] = HI_FALSE;
    }
    
    for (i = 0; i < pAttr->u32Count; i++)
    {
        if (0 != pAttr->u32PhyAddr[i])
        {
            pChn->bBufFree[i] = HI_TRUE;
        }
    }

    memcpy(&pChn->stPcivAttr, pAttr, sizeof(PCIV_ATTR_S));
    
    g_u64RdoneTime[pcivChn]   = 0;
    g_u32RdoneGap[pcivChn]    = 0;
    g_u32MaxRdoneGap[pcivChn] = 0;
    g_u32MinRdoneGap[pcivChn] = 0;
    g_u64WdoneTime[pcivChn]   = 0;
    g_u32WdoneGap[pcivChn]    = 0;
    g_u32MaxWdoneGap[pcivChn] = 0;
    g_u32MinWdoneGap[pcivChn] = 0;

    PCIV_SPIN_UNLOCK;

    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);

    PCIV_TRACE(HI_DBG_INFO, "pciv chn %d create ok\n", pcivChn);
    return s32Ret;
}

HI_S32 PCIV_Destroy(PCIV_CHN pcivChn)
{
    PCIV_CHANNEL_S *pChn = NULL;
    unsigned long   flags;

    PCIV_CHECK_CHNID(pcivChn);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);
    PCIV_SPIN_LOCK;
    pChn = &g_stPcivChn[pcivChn];
    if(HI_FALSE == pChn->bCreate)
    {
        PCIV_TRACE(HI_DBG_NOTICE, "Channel %d has not been created\n", pcivChn);

        PCIV_SPIN_UNLOCK;
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_SUCCESS;
    }
    if(HI_TRUE == pChn->bStart)
    {
        PCIV_TRACE(HI_DBG_ERR, "pciv chn%d should stop first then destroy \n", pcivChn);

        PCIV_SPIN_UNLOCK;
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_NOT_PERM;
    }
    
    PCIV_SPIN_UNLOCK;

    (HI_VOID)PCIV_FirmWareDestroy(pcivChn);
    
    PCIV_SPIN_LOCK;
    pChn->bCreate       = HI_FALSE;
    PCIV_SPIN_UNLOCK;

    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);

    PCIV_TRACE(HI_DBG_INFO, "pciv chn %d destroy ok\n", pcivChn);
    return HI_SUCCESS;
}

HI_S32 PCIV_SetAttr(PCIV_CHN pcivChn, PCIV_ATTR_S *pAttr)
{
    PCIV_CHANNEL_S *pChn = NULL;
    HI_S32          s32Ret;
    unsigned long   flags;
	HI_S32          s32LocalId = 0;
	
    PCIV_CHECK_CHNID(pcivChn);
    PCIV_CHECK_PTR(pAttr);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);

    pChn = &g_stPcivChn[pcivChn];
    if(HI_FALSE == pChn->bCreate)
    {
        PCIV_TRACE(HI_DBG_ERR, "Channel %d has not been created\n", pcivChn);

        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_UNEXIST;
    }

    if(HI_TRUE == pChn->bStart)
    {
        PCIV_TRACE(HI_DBG_ERR, "Channel %d is running\n", pcivChn);

        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_NOT_PERM;
    }

    if(HI_SUCCESS != PcivCheckAttr(pAttr))
    {
        PCIV_TRACE(HI_DBG_ERR, "Channel %d atttibute error\n", pcivChn);

        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_ILLEGAL_PARAM;
    }

	s32LocalId = PCIV_DrvAdp_GetLocalId();
    s32Ret = PCIV_FirmWareSetAttr(pcivChn, pAttr, s32LocalId);
    if (HI_SUCCESS != s32Ret)
    {
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return s32Ret;
    }
    
    PCIV_SPIN_LOCK;
    memcpy(&pChn->stPcivAttr, pAttr, sizeof(PCIV_ATTR_S));
    pChn->bConfig = HI_TRUE;
    PCIV_SPIN_UNLOCK;

    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);

    PCIV_TRACE(HI_DBG_INFO, "pciv chn %d set attr ok\n", pcivChn);
    return HI_SUCCESS;
}

HI_S32 PCIV_GetAttr(PCIV_CHN pcivChn, PCIV_ATTR_S *pAttr)
{
    PCIV_CHANNEL_S *pChn = NULL;

    PCIV_CHECK_CHNID(pcivChn);
    PCIV_CHECK_PTR(pAttr);

    pChn = &g_stPcivChn[pcivChn];
    if(HI_TRUE != pChn->bConfig)
    {
        PCIV_TRACE(HI_DBG_WARN, "attr of Channel %d has not been setted\n", pcivChn);

        return HI_ERR_PCIV_NOT_PERM;
    }

    memcpy(pAttr, &pChn->stPcivAttr, sizeof(PCIV_ATTR_S));

    return HI_SUCCESS;
}

HI_S32 PCIV_SetPreProcCfg(PCIV_CHN pcivChn, PCIV_PREPROC_CFG_S *pAttr)
{
    HI_S32          s32Ret;
    PCIV_CHECK_CHNID(pcivChn);
    PCIV_CHECK_PTR(pAttr);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);
    
    s32Ret = PCIV_FirmWareSetPreProcCfg(pcivChn, pAttr);
    
    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
    
    return s32Ret;
}

HI_S32 PCIV_GetPreProcCfg(PCIV_CHN pcivChn, PCIV_PREPROC_CFG_S *pAttr)
{
    PCIV_CHECK_CHNID(pcivChn);
    PCIV_CHECK_PTR(pAttr);
    
    return PCIV_FirmWareGetPreProcCfg(pcivChn, pAttr);
}


HI_S32 PCIV_Start(PCIV_CHN pcivChn)
{
    HI_U32            i;
    PCIV_CHANNEL_S    *pChn = NULL;
    HI_S32            s32Ret;
    HI_S32            s32LocalId = 0;
    PCIV_REMOTE_OBJ_S stRemoteObj;
    PCIV_RECVPIC_S    stRecvPic;
    unsigned long     flags;

    PCIV_CHECK_CHNID(pcivChn);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);
    PCIV_SPIN_LOCK;

    pChn = &g_stPcivChn[pcivChn];
    if(HI_TRUE != pChn->bCreate)
    {
        PCIV_TRACE(HI_DBG_ERR, "Channel %d not create\n", pcivChn);

        PCIV_SPIN_UNLOCK;
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_UNEXIST;
    }

    if(HI_TRUE == pChn->bStart)
    {
        PCIV_TRACE(HI_DBG_INFO, "Channel %d is running\n", pcivChn);

        PCIV_SPIN_UNLOCK;
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_SUCCESS;
    }

    stRemoteObj.pcivChn   = pcivChn;
    stRemoteObj.s32ChipId = 0;
    stRecvPic.enFiled     = -1;
    s32LocalId = PCIV_DrvAdp_GetLocalId();
    if (0 != s32LocalId)
    {
        for (i = 0; i < pChn->stPcivAttr.u32Count; i++)
        {
            pChn->bBufFree[i] = HI_TRUE;
        }
        s32Ret = PcivDrvAdpSendMsg(&stRemoteObj, PCIV_MSGTYPE_FREE, &stRecvPic);
    }
    
    PCIV_SPIN_UNLOCK;    
    s32Ret = PCIV_FirmWareStart(pcivChn);    
    PCIV_SPIN_LOCK;
    pChn->bStart = HI_TRUE;

    PCIV_SPIN_UNLOCK;
    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);

    PCIV_TRACE(HI_DBG_INFO, "pciv chn %d start ok\n", pcivChn);
    return HI_SUCCESS;
}

HI_S32 PCIV_Stop(PCIV_CHN pcivChn)
{
    PCIV_CHANNEL_S *pChn = NULL;
    HI_S32          s32Ret;
    unsigned long   flags;
    HI_S32          s32LocalId = 0;

    PCIV_CHECK_CHNID(pcivChn);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);

    pChn = &g_stPcivChn[pcivChn];

    if(HI_TRUE != pChn->bStart)
    {
        PCIV_TRACE(HI_DBG_INFO, "Channel %d has stoped\n", pcivChn);

        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_SUCCESS;
    }

    PCIV_SPIN_LOCK;
    /* 先置停止标志 */
    pChn->bStart = HI_FALSE;
    PCIV_SPIN_UNLOCK;

    /* 等待PCI任务完成 */
    s32LocalId = PCIV_DrvAdp_GetLocalId();
    if(0 != s32LocalId)
    {
        while(pChn->u32SendCnt != pChn->u32RespCnt)
        {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(2);
            continue;
        }

    }

    /* 停止媒体业务 */
    s32Ret = PCIV_FirmWareStop(pcivChn);
    if (s32Ret)
    {
        PCIV_SPIN_LOCK;
        pChn->bStart = HI_TRUE;
        PCIV_SPIN_UNLOCK;

        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return s32Ret;
    }

    PCIV_SPIN_LOCK;
    pChn->bCanRecv      = HI_FALSE;
    PCIV_SPIN_UNLOCK;

    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);

    PCIV_TRACE(HI_DBG_INFO, "pciv chn %d stop ok\n", pcivChn);
    return HI_SUCCESS;
}

HI_S32 PCIV_Hide(PCIV_CHN pcivChn, HI_BOOL bHide)
{
    PCIV_CHANNEL_S *pChn = NULL;

    PCIV_CHECK_CHNID(pcivChn);

    PCIV_MUTEX_DOWN(g_stPcivChn[pcivChn].pcivMutex);
    
    pChn = &g_stPcivChn[pcivChn];
    if(HI_TRUE != pChn->bCreate)
    {
        PCIV_TRACE(HI_DBG_ERR, "Channel %d not created\n", pcivChn);
        PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
        return HI_ERR_PCIV_UNEXIST;
    }

    pChn->bHide = bHide;
    
    PCIV_MUTEX_UP(g_stPcivChn[pcivChn].pcivMutex);
    
    PCIV_TRACE(HI_DBG_INFO, "pciv chn %d hide%d ok\n", pcivChn, bHide);
    return HI_SUCCESS;
}


/* 只有从片接收数据或图象时，才需要配置Window Vb */
HI_S32 PCIV_WindowVbCreate(PCIV_WINVBCFG_S *pCfg)
{
    PCIV_WINVBCFG_S stVbCfg;
    HI_U32 i, j, u32Size, u32Count;
    HI_S32 s32LocalId = PCIV_DrvAdp_GetLocalId();

    /* 主片上不支持创建专用区域 */
    if(0 == s32LocalId) 
    {
        return HI_ERR_PCIV_NOT_SUPPORT;
    }

    /* 对缓存池进行从小到大排序，方便后续的使用 */
    memcpy(&stVbCfg, pCfg, sizeof(stVbCfg));
    for(i = 0; i < pCfg->u32PoolCount - 1; i++)
    {
        for(j = i+1; j < pCfg->u32PoolCount; j++)
        {
            if(stVbCfg.u32BlkSize[j] < stVbCfg.u32BlkSize[i])
            {
                u32Size  = stVbCfg.u32BlkSize[i];
                u32Count = stVbCfg.u32BlkCount[i];

                stVbCfg.u32BlkSize[i]  = stVbCfg.u32BlkSize[j];
                stVbCfg.u32BlkCount[i] = stVbCfg.u32BlkCount[j];

                stVbCfg.u32BlkSize[j]  = u32Size;
                stVbCfg.u32BlkCount[j] = u32Count;
            }
        }
    }

    return PCIV_FirmWareWindowVbCreate(&stVbCfg);
}

HI_S32 PCIV_WindowVbDestroy(HI_VOID)
{
    HI_S32 s32LocalId = PCIV_DrvAdp_GetLocalId();

    /* 主片上不支持销毁专用区域 */
    if(0 == s32LocalId) 
    {
        return HI_ERR_PCIV_NOT_SUPPORT;
    }
    
    return PCIV_FirmWareWindowVbDestroy();
}


HI_S32 PCIV_Malloc(HI_U32 u32Size, HI_U32 *pPhyAddr)
{
    HI_S32 s32LocalId = PCIV_DrvAdp_GetLocalId();

    return PCIV_FirmWareMalloc(u32Size, s32LocalId, pPhyAddr);
}

HI_S32 PCIV_Free(HI_U32 u32PhyAddr)
{
    return PCIV_FirmWareFree(u32PhyAddr);
}

HI_S32 PCIV_MallocChnBuffer(PCIV_CHN pcivChn, HI_U32 u32Index, HI_U32 u32Size, HI_U32 *pPhyAddr)
{
    HI_S32 s32LocalId = PCIV_DrvAdp_GetLocalId();

    return PCIV_FirmWareMallocChnBuffer(pcivChn, u32Index, u32Size, s32LocalId, pPhyAddr);
}

HI_S32 PCIV_FreeChnBuffer(PCIV_CHN pcivChn, HI_U32 u32Index)
{
	HI_S32 s32LocalId = PCIV_DrvAdp_GetLocalId();
	
    return PCIV_FirmWareFreeChnBuffer(pcivChn, u32Index, s32LocalId);
}

HI_VOID  PcivUserDmaDone(PCIV_SENDTASK_S *pstTask)
{
    PCIV_USERDMANODE_S *pUserDmaNode = NULL;

    /* 断言完成的DMA任务的确是最后一个 */
    HI_ASSERT((pstTask->u32PrvData[0] + 1) == pstTask->u32PrvData[1]);

    pUserDmaNode = (PCIV_USERDMANODE_S *)pstTask->u32PrvData[2];
    pUserDmaNode->bDmaDone = HI_TRUE;
    wake_up(&pUserDmaNode->stwqDmaDone);
}

HI_S32 PCIV_UserDmaTask(PCIV_DMA_TASK_S *pTask)
{
    HI_S32              i;
    HI_S32              s32Ret = HI_SUCCESS;
    unsigned long       flags;
    PCIV_SENDTASK_S     stPciTask;
    PCIV_USERDMANODE_S *pUserDmaNode = NULL;

    if(list_empty(&g_listHeadUserDma))
    {
        return HI_ERR_PCIV_BUSY;
    }


    PCIV_SPIN_LOCK;
    pUserDmaNode = list_entry(g_listHeadUserDma.next, PCIV_USERDMANODE_S, list);
    list_del(g_listHeadUserDma.next);
    PCIV_SPIN_UNLOCK;

    pUserDmaNode->bDmaDone = HI_FALSE;

    for(i=0; i<pTask->u32Count; i++)
    {
        stPciTask.u32SrcPhyAddr = pTask->pBlock[i].u32SrcAddr;
        stPciTask.u32DstPhyAddr = pTask->pBlock[i].u32DstAddr;
        stPciTask.u32Len        = pTask->pBlock[i].u32BlkSize;
        stPciTask.bRead         = pTask->bRead;
        stPciTask.u32PrvData[0] = i;
        stPciTask.u32PrvData[1] = pTask->u32Count;
        stPciTask.u32PrvData[2] = (HI_U32)pUserDmaNode;
        stPciTask.pCallBack     = NULL;

        /* If this is the last task node, we set the callback */
        if( (i+1) == pTask->u32Count)
        {
            stPciTask.pCallBack = PcivUserDmaDone;
        }

        if(HI_SUCCESS != PCIV_DrvAdp_AddDmaTask(&stPciTask))
        {
            s32Ret     = HI_ERR_PCIV_NOMEM;
            break;
        }
    }

    if(HI_SUCCESS == s32Ret)
    {
        HI_S32 s32TimeLeft;
        s32TimeLeft = wait_event_timeout(pUserDmaNode->stwqDmaDone,
                                      (pUserDmaNode->bDmaDone == HI_TRUE), 200);
        if(0 == s32TimeLeft)
        {
            s32Ret = HI_ERR_PCIV_TIMEOUT;
        }
    }

    PCIV_SPIN_LOCK;
    list_add_tail(&pUserDmaNode->list, &g_listHeadUserDma);
    PCIV_SPIN_UNLOCK;

    return s32Ret;
}

static HI_VOID PcivCheckNotifyCnt(PCIV_CHN pcivChn, HI_U32 u32Index, HI_U32 u32Count)
{
    PCIV_CHANNEL_S *pstPcivChn = &g_stPcivChn[pcivChn];

    if(0 == u32Count)
    {
        pstPcivChn->u32NotifyCnt = 0;
    }
    else
    {
        pstPcivChn->u32NotifyCnt ++;
        if(u32Count != pstPcivChn->u32NotifyCnt)
        {
            printk("Warnning: PcivChn:%d, ReadDone MsgSeq -> (%d,%d),bufindex:%d \n",
                pcivChn, u32Count, pstPcivChn->u32NotifyCnt, u32Index);
            //HI_ASSERT(0);
        }
    }
}

/* 收到远端的共享内存释放消息时，调用此接口将内存状态置为空闲 */
HI_S32 PCIV_FreeShareBuf(PCIV_CHN pcivChn, HI_U32 u32Index, HI_U32 u32Count)
{
    PCIV_CHANNEL_S    *pstPcivChn;
    unsigned long flags;

    PCIV_CHECK_CHNID(pcivChn);

    pstPcivChn = &g_stPcivChn[pcivChn];

    PCIV_SPIN_LOCK;
    if(HI_TRUE != pstPcivChn->bStart)
    {
        PCIV_TRACE(HI_DBG_ERR, "Pciv channel %d not start!\n", pcivChn);
        PCIV_SPIN_UNLOCK;
        return HI_ERR_PCIV_UNEXIST;
    }

    if(u32Index >= PCIV_MAX_BUF_NUM)
    {
        PCIV_TRACE(HI_DBG_ERR, "Buffer index %d is too larger!\n", u32Index);
        PCIV_SPIN_UNLOCK;
        return HI_ERR_PCIV_ILLEGAL_PARAM;
    }
    HI_ASSERT(u32Index < pstPcivChn->stPcivAttr.u32Count);

    /* 检测消息中序号与本地序号是否一致 */
    PcivCheckNotifyCnt(pcivChn, u32Index, u32Count);

    /* 置buffer状态为空闲 */
    pstPcivChn->bBufFree[u32Index] = HI_TRUE;
    PCIV_SPIN_UNLOCK;
    
    return HI_SUCCESS;
}

HI_S32 PCIV_FreeAllBuf(PCIV_CHN pcivChn)
{
    HI_U32        i;
    unsigned long flags;
    
    PCIV_SPIN_LOCK;
    if (HI_TRUE != g_stPcivChn[pcivChn].bCreate)
    {
        PCIV_SPIN_UNLOCK;
        return HI_SUCCESS;
    }
    
    for (i = 0; i < g_stPcivChn[pcivChn].stPcivAttr.u32Count; i++)
    {
        g_stPcivChn[pcivChn].bBufFree[i] = HI_TRUE;
    }

    g_stPcivChn[pcivChn].bCanRecv = HI_TRUE;
    PCIV_SPIN_UNLOCK;
    return HI_SUCCESS;
}


/* 启动DMA传输时，必须先调用此接口获取一块可用的共享内存 */
static HI_S32 PCIV_GetShareBuf(PCIV_CHN pcivChn, HI_U32 *pCurIndex)
{
    HI_U32          i;
    PCIV_CHANNEL_S *pstPcivChn;

    pstPcivChn = &g_stPcivChn[pcivChn];
    for(i=0; i<pstPcivChn->stPcivAttr.u32Count; i++)
    {
        if(pstPcivChn->bBufFree[i])
        {
            *pCurIndex = i;
            return HI_SUCCESS;
        }
    }

    return HI_FAILURE;
}

static HI_S32 PCIV_GetShareBufState(PCIV_CHN pcivChn)
{
    HI_U32          i;
    PCIV_CHANNEL_S *pstPcivChn;

    pstPcivChn = &g_stPcivChn[pcivChn];
    for(i=0; i<pstPcivChn->stPcivAttr.u32Count; i++)
    {
        if(pstPcivChn->bBufFree[i])
        {
            return HI_SUCCESS;
        }
    }

    return HI_FAILURE;
}


HI_VOID PcivSrcPicSendDone(PCIV_SENDTASK_S *pTask)
{
    PCIV_CHN        pcivChn;
    PCIV_CHANNEL_S *pstChn;
    PCIV_RECVPIC_S  stRecvPic;
    PCIV_SRCPIC_S   stSrcPic;

    pcivChn = pTask->u32PrvData[0];
    HI_ASSERT((pcivChn >= 0) && (pcivChn < PCIV_MAX_CHN_NUM));

    pstChn = &g_stPcivChn[pcivChn];

    memcpy(&stRecvPic, (PCIV_RECVPIC_S*)pTask->pvPrvData, sizeof(PCIV_RECVPIC_S));
    kfree(pTask->pvPrvData);

    (void)PCIV_DrvAdp_DmaEndNotify(&pstChn->stPcivAttr.stRemoteObj, &stRecvPic);

    stSrcPic.u32PhyAddr = pTask->u32SrcPhyAddr;
    stSrcPic.u32PoolId  = pTask->u32PrvData[1];
    (HI_VOID)PCIV_SrcPicFree(pcivChn, &stSrcPic);

    pstChn->u32RespCnt++;

    return;
}

/* PCIV Firmware 处理完源图像后 自动调用此接口将图像通过PCI DMA发送到 PCI 目标端 */
HI_S32 PCIV_SrcPicSend(PCIV_CHN pcivChn, PCIV_SRCPIC_S *pSrcPic, VI_MIXCAP_STAT_S *pstMixCapState)
{
    PCIV_CHANNEL_S  *pChn = &g_stPcivChn[pcivChn];
    PCIV_SENDTASK_S  stPciTask;
    HI_U32           u32CurIndex;
    HI_S32           s32Ret;
    PCIV_RECVPIC_S  *pstRecvPic = NULL;
    unsigned long    flags;

    PCIV_SPIN_LOCK;//注意防止pciv和vfwm相互调用的互锁可能

    if(HI_TRUE != pChn->bStart)
    {
        PCIV_SPIN_UNLOCK;
        printk("Warning: pciv is not enable, when func: %s \n\n", __FUNCTION__);
        return HI_FAILURE;
    }

    HI_ASSERT(pSrcPic->enSrcType < PCIV_BIND_BUTT);
    HI_ASSERT(pSrcPic->enFiled < VIDEO_FIELD_BUTT);

    pChn->u32GetCnt++;
    
    s32Ret = PCIV_GetShareBuf(pcivChn, &u32CurIndex);
    if(HI_SUCCESS != s32Ret)
    {
        if((PCIV_BIND_VI == pSrcPic->enSrcType) || (PCIV_BIND_VO == pSrcPic->enSrcType))
        {
            pChn->u32LostCnt ++;
        }
        PCIV_SPIN_UNLOCK;
		PCIV_TRACE(HI_DBG_ERR, "No Free Buf, chn%d\n", pcivChn);
        return HI_ERR_PCIV_NOBUF;
    }

    pstRecvPic = (PCIV_RECVPIC_S*)kmalloc(sizeof(PCIV_RECVPIC_S), GFP_ATOMIC);
    if (NULL == pstRecvPic)
    {
        PCIV_TRACE(HI_DBG_EMERG, "kmalloc PCIV_RECVPIC_S err! chn%d\n", pcivChn);
        pChn->u32LostCnt ++;
        PCIV_SPIN_UNLOCK;
        return HI_ERR_PCIV_NOMEM;
    }

    pstRecvPic->u32Index                    = u32CurIndex;
    pstRecvPic->u32Count                    = pChn->u32SendCnt;
    pstRecvPic->u64Pts                      = pSrcPic->u64Pts;
    pstRecvPic->u32TimeRef                  = pSrcPic->u32TimeRef;
    pstRecvPic->enSrcType                   = pSrcPic->enSrcType;
    pstRecvPic->enFiled                     = pSrcPic->enFiled;
    pstRecvPic->stMixCapState.bHasDownScale = pstMixCapState->bHasDownScale;
    pstRecvPic->stMixCapState.bMixCapMode   = pstMixCapState->bMixCapMode;
    pstRecvPic->bBlock                      = pSrcPic->bBlock;
        
    /* 隐藏通道即不将图像DMA发送到对端，而只进行消息通讯 */
    stPciTask.u32Len        = (!pChn->bHide) ? (pChn->stPcivAttr.u32BlkSize) : 0;
    stPciTask.u32SrcPhyAddr = pSrcPic->u32PhyAddr;
    stPciTask.u32DstPhyAddr = pChn->stPcivAttr.u32PhyAddr[u32CurIndex];
    stPciTask.bRead         = HI_FALSE;
    stPciTask.u32PrvData[0] = pcivChn;              /* 通道号 */
    stPciTask.u32PrvData[1] = pSrcPic->u32PoolId;   /* 输入源的PoolID */

    stPciTask.pvPrvData     = (HI_VOID*)pstRecvPic;
    stPciTask.pCallBack     = PcivSrcPicSendDone;   /* 注册PCI DMA 传输完成后的回调 */
    s32Ret = PCIV_DrvAdp_AddDmaTask(&stPciTask);
    if(HI_SUCCESS != s32Ret)
    {
        PCIV_TRACE(HI_DBG_EMERG, "DMA task err! chn%d\n", pcivChn);
        kfree(pstRecvPic);
        pChn->u32LostCnt ++;
        PCIV_SPIN_UNLOCK;
        return s32Ret;
    }

    /* 将此序号的缓存置为非空闲状态 */
    HI_ASSERT(HI_TRUE == pChn->bBufFree[u32CurIndex]);
    pChn->bBufFree[u32CurIndex] = HI_FALSE;

    pChn->u32SendCnt++;

    PCIV_SPIN_UNLOCK;

    return HI_SUCCESS;
}

/* PCI DMA发送完毕后，需要调度此接口将图象缓存释放 */
HI_S32 PCIV_SrcPicFree(PCIV_CHN pcivChn, PCIV_SRCPIC_S *pSrcPic)
{
    return PCIV_FirmWareSrcPicFree(pcivChn, pSrcPic);
}

/* PCIV 收到远端图象后，调用此接口将图象送给VO显示/VPSS/VENC使用 */
HI_S32 PCIV_ReceivePic(PCIV_CHN pcivChn, PCIV_RECVPIC_S *pRecvPic)
{
    HI_S32 s32Ret;
    unsigned long flags;

    HI_ASSERT(pcivChn < PCIV_MAX_CHN_NUM);
    HI_ASSERT(pRecvPic->enFiled < VIDEO_FIELD_BUTT);
    HI_ASSERT(pRecvPic->enSrcType < PCIV_BIND_BUTT);
    HI_ASSERT(pRecvPic->u32Index < g_stPcivChn[pcivChn].stPcivAttr.u32Count);

    PCIV_SPIN_LOCK;
    /************************************************************************************
     * 【HSCP201308020003】l00181524,2013.08.16,两片反复销毁创建时，存在消息缓存在pcie中的可能
     * 导致主片还没起来，或刚创建就接收到从片销毁之前的图片，占用了buf，而从片该索引号的buf是空的
     * 这样就会导致从片在重新创建后使用该索引的buf，而主片的已被占用，就出现assert了。
     * 引入bCanRecv标志位，当从片重新start时，会发一条消息过来，让主片释放所有vb，保持与从片的同步
     * 由于收消息的机制是用软中断触发，3531是双核，因此需要bCanRecv来做双核互斥。
     **************************************************************************************/
    if ((HI_TRUE != g_stPcivChn[pcivChn].bStart) || (HI_TRUE != g_stPcivChn[pcivChn].bCanRecv))
    {
    	g_stPcivChn[pcivChn].bBufFree[pRecvPic->u32Index] = HI_FALSE;
        PCIV_SPIN_UNLOCK;
		PCIV_FirmWareRecvPicFree(pcivChn);
        PCIV_TRACE(HI_DBG_ERR, "pcivChn:%d hasn't be ready to receive pic, bStart: %d, bCanRecv: %d\n", 
        pcivChn, g_stPcivChn[pcivChn].bStart, g_stPcivChn[pcivChn].bStart);
        return HI_ERR_PCIV_SYS_NOTREADY;//考虑解码图像情形
    }

    g_stPcivChn[pcivChn].u32GetCnt++;

    /* 之前图像Buffer应该是空闲状态 */
    /* 之前图像Buffer应该是空闲状态.注意，如果两片见的调用顺序不合理，上层应用没有确保两片都返回正确的消息后就强行重新创建，就可能会触发此处assert */
    HI_ASSERT(HI_TRUE == g_stPcivChn[pcivChn].bBufFree[pRecvPic->u32Index]);

    /* 不管会不会成功送给VO显示，这里都置成非空闲状态 */
    g_stPcivChn[pcivChn].bBufFree[pRecvPic->u32Index] = HI_FALSE;
    PCIV_SPIN_UNLOCK;

    /* 调用Firmware层接口，将图像发送到VO显示 */
    s32Ret = PCIV_FirmWareRecvPicAndSend(pcivChn, pRecvPic);
    if(HI_SUCCESS != s32Ret)
    {
        g_stPcivChn[pcivChn].u32LostCnt++;
        PCIV_TRACE(HI_DBG_ERR, "PCIV_FirmWareRecvPicAndSend Err,chn:%d, return value: 0x%x \n", pcivChn, s32Ret);
        return s32Ret;
    }

    g_stPcivChn[pcivChn].u32SendCnt++;
    return HI_SUCCESS;
}

/* VO/VPSS/VENC使用完成后，Firmware会自动调度此接口，将图象缓存返回 */
HI_S32 PCIV_RecvPicFree(PCIV_CHN pcivChn, PCIV_RECVPIC_S *pRecvPic)
{
    HI_S32 s32Ret = HI_SUCCESS;
    PCIV_CHANNEL_S *pChn = NULL;

    PCIV_CHECK_CHNID(pcivChn);

    pChn = &g_stPcivChn[pcivChn];

    HI_ASSERT(pRecvPic->u32Index < pChn->stPcivAttr.u32Count);
    HI_ASSERT(pRecvPic->u64Pts == 0);

    /* 只有已置为使用状态的缓存，才执行释放动作 */
    if(HI_TRUE != pChn->bBufFree[pRecvPic->u32Index])
    {
        /* Buffer状态置为空闲状态*/
        pChn->bBufFree[pRecvPic->u32Index] = HI_TRUE;

        pRecvPic->u32Count = pChn->u32RespCnt;

        /* 发送READDONE 消息给发送端通知其执行释放资源的相关操作 */
        s32Ret = PCIV_DrvAdp_BufFreeNotify(&pChn->stPcivAttr.stRemoteObj, pRecvPic);
        if(HI_SUCCESS != s32Ret)
        {
            PCIV_TRACE(HI_DBG_ERR, "PCIV_DrvAdp_BufFreeNotify err,chn%d\n", pcivChn);
            return s32Ret;
        }
        pChn->u32RespCnt++;
    }

    return HI_SUCCESS;
}

HI_S32 PCIV_Init(void)
{
    HI_S32 i;
    PCIVFMW_CALLBACK_S stFirmWareCallBack;

    spin_lock_init(&g_PcivLock);

    INIT_LIST_HEAD(&g_listHeadUserDma);
    for(i=0; i<PCIV_MAX_DMATASK; i++)
    {
        init_waitqueue_head(&g_stUserDmaPool[i].stwqDmaDone);
        g_stUserDmaPool[i].bDmaDone = HI_TRUE;
        list_add_tail(&g_stUserDmaPool[i].list, &g_listHeadUserDma);
    }

    memset(g_stPcivChn, 0, sizeof(g_stPcivChn));
    for(i = 0; i < PCIV_MAX_CHN_NUM; i++)
    {
        g_stPcivChn[i].bCreate = HI_FALSE;
        g_stPcivChn[i].stPcivAttr.stRemoteObj.s32ChipId = -1;

        sema_init(&g_stPcivChn[i].pcivMutex, 1);
    }

    stFirmWareCallBack.pfSrcSendPic                = PCIV_SrcPicSend;
    stFirmWareCallBack.pfRecvPicFree               = PCIV_RecvPicFree;
    stFirmWareCallBack.pfQueryPcivChnShareBufState = PCIV_GetShareBufState;
    (HI_VOID)PCIV_FirmWareRegisterFunc(&stFirmWareCallBack);

    PCIV_DrvAdp_Init();

    return HI_SUCCESS;
}

HI_VOID  PCIV_Exit(void)
{
    HI_S32 i, s32Ret;

    for (i = 0; i < PCIV_MAX_CHN_NUM; i++)
    {
        if (HI_TRUE != g_stPcivChn[i].bCreate) 
        {
            msleep(10);
            continue;
        }
        
        s32Ret = PCIV_Stop(i);
        if(HI_SUCCESS != s32Ret)
        {
            PCIV_TRACE(HI_DBG_ERR, "PCIV_Stop err,chn%d\n", i);
            return;
        }
        
        s32Ret = PCIV_Destroy(i);
        if(HI_SUCCESS != s32Ret)
        {
            PCIV_TRACE(HI_DBG_ERR, "PCIV_Destroy err,chn%d\n", i);
            return;
        }
        
    }

    PCIV_DrvAdp_Exit();
    return;
}

inline static HI_CHAR* PRINT_PIXFORMAT(PIXEL_FORMAT_E enPixFormt)
{
    if (PIXEL_FORMAT_YUV_SEMIPLANAR_420 == enPixFormt)
    {
        return "sp420";
    }
    else if (PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixFormt)
    {
        return "sp422";
    }
    else if (PIXEL_FORMAT_YUV_PLANAR_420 == enPixFormt)
    {
        return "p420";
    }
    else if (PIXEL_FORMAT_YUV_PLANAR_422 == enPixFormt)
    {
        return "p422";
    }
    else if (PIXEL_FORMAT_UYVY_PACKAGE_422 == enPixFormt)
    {
        return "uyvy422";
    }
    else if (PIXEL_FORMAT_YUYV_PACKAGE_422 == enPixFormt)
    {
        return "yuyv422";
    }
    else if (PIXEL_FORMAT_VYUY_PACKAGE_422 == enPixFormt)
    {
        return "vyuy422";
    }
    else
    {
        return NULL;
    }
}
HI_CHAR* PRINT_FIELD(VIDEO_FIELD_E enField)
{
    if (VIDEO_FIELD_TOP == enField) 
    {
        return "top";
    }
    else if (VIDEO_FIELD_BOTTOM == enField) 
    {
        return "bot";
    }
    else if (VIDEO_FIELD_FRAME == enField) 
    {
        return "frm";
    }
    else if (VIDEO_FIELD_INTERLACED == enField) 
    {
        return "intl";
    }
    else 
    {
        return NULL;
    }
}


HI_S32 PCIV_ProcShow(struct seq_file *s, HI_VOID *pArg)
{
    HI_S32 i;
    PCIV_CHANNEL_S  *pstPcivChn;
    PCIV_ATTR_S     *pstAttr;
    PCIV_CHN         pcivChn;
    HI_CHAR          cString[PCIV_MAX_BUF_NUM*2+1] = {0};

    seq_printf(s, "\n[PCIV] Version: ["MPP_VERSION"], Build Time:["__DATE__", "__TIME__"]\n");

    seq_printf(s, "\n-----PCIV CHN ATTR--------------------------------------------------------------\n");
    seq_printf(s, "%6s"    "%8s"   "%8s"    "%8s"     "%8s"   "%8s"    "%8s"    "%8s"     "%10s" "\n"
                 ,"PciChn","Width","Height","Stride0","Field","PixFmt","BufCnt","BufSize","PhyAddr0");
    for (pcivChn = 0; pcivChn < PCIV_MAX_CHN_NUM; pcivChn++)
    {
        pstPcivChn = &g_stPcivChn[pcivChn];

        if (HI_FALSE == pstPcivChn->bCreate) continue;

        pstAttr = &pstPcivChn->stPcivAttr;
        seq_printf(s, "%6d" "%8d" "%8d" "%8d" "%8s" "%8s" "%8d" "%8d" "%10x" "\n",
            pcivChn,
            pstAttr->stPicAttr.u32Width,
            pstAttr->stPicAttr.u32Height,
            pstAttr->stPicAttr.u32Stride[0],
            PRINT_FIELD(pstAttr->stPicAttr.u32Field),
            PRINT_PIXFORMAT(pstAttr->stPicAttr.enPixelFormat),
            pstAttr->u32Count,
            pstAttr->u32BlkSize,
            pstAttr->u32PhyAddr[0]);
    }

    seq_printf(s, "\n-----PCIV CHN STATUS------------------------------------------------------------\n");
    seq_printf(s, "%6s"    "%8s"     "%8s"     "%8s"    "%8s"     "%8s"     "%8s"     "%8s"     "%12s"    "\n"
                 ,"PciChn","RemtChp","RemtChn","GetCnt","SendCnt","RespCnt","LostCnt","NtfyCnt","BufBusy");
    for (pcivChn = 0; pcivChn < PCIV_MAX_CHN_NUM; pcivChn++)
    {
        pstPcivChn = &g_stPcivChn[pcivChn];
        if (HI_FALSE == pstPcivChn->bCreate) continue;

        for(i=0; i<pstPcivChn->stPcivAttr.u32Count; i++)
        {
            sprintf(&cString[i*2], "%2d", !pstPcivChn->bBufFree[i]);
        }
        seq_printf(s, "%6d" "%8d" "%8d" "%8d" "%8d" "%8d" "%8d" "%8d" "%12s" "\n",
            pcivChn,
            pstPcivChn->stPcivAttr.stRemoteObj.s32ChipId,
            pstPcivChn->stPcivAttr.stRemoteObj.pcivChn,
            pstPcivChn->u32GetCnt,
            pstPcivChn->u32SendCnt,
            pstPcivChn->u32RespCnt,
            pstPcivChn->u32LostCnt,
            pstPcivChn->u32NotifyCnt,
            cString);
    }

    seq_printf(s, "\n-----PCIV MSG STATUS------------------------------------------------------------\n");
    seq_printf(s, "%6s"    "%10s"     "%10s"     "%10s"     "%10s"     "%10s"     "%10s"    "\n"
                 ,"PciChn","RdoneGap","MaxRDGap","MinRDGap","WdoneGap","MaxWDGap","MinWDGap");
    for (pcivChn = 0; pcivChn < PCIV_MAX_CHN_NUM; pcivChn++)
    {
        pstPcivChn = &g_stPcivChn[pcivChn];
        if (HI_FALSE == pstPcivChn->bCreate) continue;

        seq_printf(s, "%6d" "%10d" "%10d" "%10d" "%10d" "%10d" "%10d" "\n",
            pcivChn,
            g_u32RdoneGap[pcivChn],
            g_u32MaxRdoneGap[pcivChn],
            g_u32MinRdoneGap[pcivChn],
            g_u32WdoneGap[pcivChn],
            g_u32MaxWdoneGap[pcivChn],
            g_u32MinWdoneGap[pcivChn]);
    }

    return 0;
}


