#include "mm_ext.h"
#include "hi_debug.h"
#include "hi_comm_video.h"
#include "pciv_pic_queue.h"


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

HI_S32 PCIV_CreatPicQueue(PCIV_PIC_QUEUE_S *pstPicQueue, HI_U32 u32MaxNum)
{
    HI_U32 i;
	PCIV_PIC_NODE_S *pstPicNode;

    /*初始化各队列头*/
    INIT_LIST_HEAD(&pstPicQueue->stFreeList);
    INIT_LIST_HEAD(&pstPicQueue->stBusyList);

    /*分配节点buffer(cached)*/
    pstPicQueue->pstNodeBuf = CMPI_Kmalloc(u32MaxNum * sizeof(PCIV_PIC_NODE_S));
    if (HI_NULL == pstPicQueue->pstNodeBuf)
    {
        return HI_FAILURE;
    }

    /*将节点buffer挂到free队列上*/
    pstPicNode = pstPicQueue->pstNodeBuf;

    for(i = 0; i < u32MaxNum; i++)
    {
        list_add_tail(&pstPicNode->stList, &pstPicQueue->stFreeList);
        pstPicNode++;

    }
    /*初始化队列其它元素*/
    pstPicQueue->u32FreeNum = u32MaxNum;
    pstPicQueue->u32Max = u32MaxNum;
    pstPicQueue->u32BusyNum = 0;

    return HI_SUCCESS;
}

HI_VOID PCIV_DestroyPicQueue(PCIV_PIC_QUEUE_S *pstNodeQueue)
{

    /*初始化各队列头*/
    INIT_LIST_HEAD(&pstNodeQueue->stFreeList);
    INIT_LIST_HEAD(&pstNodeQueue->stBusyList);


    pstNodeQueue->u32FreeNum = 0;
    pstNodeQueue->u32BusyNum = 0;
    pstNodeQueue->u32Max = 0;

    if (pstNodeQueue->pstNodeBuf)
    {
        CMPI_Kfree(pstNodeQueue->pstNodeBuf);
    }

    return;

}

PCIV_PIC_NODE_S *PCIV_PicQueueGetFree(PCIV_PIC_QUEUE_S *pstNodeQueue)
{
	struct list_head *plist;
	PCIV_PIC_NODE_S * pstPicNode = NULL;

	if (list_empty(&pstNodeQueue->stFreeList))
	{
		return NULL;
	}

	plist = pstNodeQueue->stFreeList.next;
    HI_ASSERT(HI_NULL != plist);
	list_del(plist);

	pstPicNode = list_entry(plist, PCIV_PIC_NODE_S, stList);

	pstNodeQueue->u32FreeNum--;
	return pstPicNode;
}

HI_VOID  PCIV_PicQueuePutBusyHead(PCIV_PIC_QUEUE_S *pstNodeQueue, PCIV_PIC_NODE_S  *pstPicNode)
{
    list_add(&pstPicNode->stList, &pstNodeQueue->stBusyList);
    pstNodeQueue->u32BusyNum++;

    return;
}

HI_VOID  PCIV_PicQueuePutBusy(PCIV_PIC_QUEUE_S *pstNodeQueue, PCIV_PIC_NODE_S  *pstPicNode)
{
    list_add_tail(&pstPicNode->stList, &pstNodeQueue->stBusyList);
    pstNodeQueue->u32BusyNum++;

    return;
}

PCIV_PIC_NODE_S *PCIV_PicQueueGetBusy(PCIV_PIC_QUEUE_S *pstNodeQueue)
{
	struct list_head *plist;
	PCIV_PIC_NODE_S * pstPicNode = NULL;

	if(list_empty(&pstNodeQueue->stBusyList))
	{
		return NULL;
	}

	plist = pstNodeQueue->stBusyList.next;
	list_del(plist);

	pstPicNode = list_entry(plist, PCIV_PIC_NODE_S, stList);

	pstNodeQueue->u32BusyNum--;
	return pstPicNode;
}
PCIV_PIC_NODE_S *PCIV_PicQueueQueryBusy(PCIV_PIC_QUEUE_S *pstNodeQueue)
{
	struct list_head *plist;
	PCIV_PIC_NODE_S * pstPicNode = NULL;

	if(list_empty(&pstNodeQueue->stBusyList))
	{
        return NULL;
	}
	plist = pstNodeQueue->stBusyList.next;
	pstPicNode = list_entry(plist, PCIV_PIC_NODE_S, stList);
	return pstPicNode;
}


 HI_VOID PCIV_PicQueuePutFree(PCIV_PIC_QUEUE_S *pstNodeQueue,PCIV_PIC_NODE_S  *pstPicNode)
{
    list_add_tail(&pstPicNode->stList, &pstNodeQueue->stFreeList);
    pstNodeQueue->u32FreeNum++;

    return;
}


HI_U32 PCIV_PicQueueGetFreeNum(PCIV_PIC_QUEUE_S *pstNodeQueue)
{
    return pstNodeQueue->u32FreeNum;
}

HI_U32 PCIV_PicQueueGetBusyNum(PCIV_PIC_QUEUE_S *pstNodeQueue)
{
    return pstNodeQueue->u32BusyNum;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


