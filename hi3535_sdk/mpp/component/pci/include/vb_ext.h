/******************************************************************************
 
Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.
 
******************************************************************************
File Name     : vb_ext.h
Version       : Initial Draft
Author        : Hisilicon multimedia software group
Created       : 2006/12/13
Description   : 
History       :
1.Date        : 2006/02/13
Author        : wangjian 
Modification: Created file
 
******************************************************************************/

#ifndef __VB_EXT_H__
#define __VB_EXT_H__

#include "hi_comm_vb.h"
#include "hi_common.h"
#include "mkp_vb.h"

typedef  HI_U32 VB_BLKHANDLE;

typedef struct HI_VB_CREATE_EX_INFO_S
{
    MPP_CHN_S    stMod;
    HI_S32      (*pCallBack)(MPP_CHN_S stMod,HI_U32 u32PhyAddr);
}VB_CREATE_EX_INFO_S;

/*****************************************************************************
 Prototype    : VB_CreatePool
 Description  : create a private VB pool
 Input        : u32BlkCnt      : count of buffer blocks
                u32BlkCnt      : size of buffer blocks
 Output       : pu32PoolId     : pointer to the ID of pool created
 Return Value : HI_S32         : 0 -- success; !0 : failure
*****************************************************************************/
extern HI_S32 VB_CreatePool(HI_U32 *pu32PoolId,HI_U32 u32BlkCnt, HI_U32 u32BlkSize, 
                HI_CHAR *pcMmzName);

/*****************************************************************************
 Prototype    : VB_CreatePoolExt
 Description  : create a private VB pool,but it's not malloc buffer
 Input        : stVbRegInfo      : register info
                u32BlkCnt      : count of buffer blocks
 Output       : pu32PoolId     : pointer to the ID of pool created
 Return Value : HI_S32         : 0 -- success; !0 : failure
*****************************************************************************/
extern HI_S32 VB_CreatePoolExt(HI_U32 *pu32PoolId,HI_U32 u32BlkCnt,VB_CREATE_EX_INFO_S *pstVbRegInfo);

/*****************************************************************************
 Prototype    : VB_AddBlkToPool
 Description  : add a buffer block to a specified pool
 Input        : u32PoolId      : ID of a pool
                u32PhysAddr    : physcial address of the buffer
                u32Uid         : ID of user getting the buffer
 Output       : 
 Return Value : VB_BLKHANDLE   : 
                success : Not VB_INVALID_HANDLE 
                failure : VB_INVALID_HANDLE 
*****************************************************************************/
extern HI_S32 VB_AddBlkToPool(HI_U32 u32PoolId,HI_U32 u32PhysAddr,HI_U32 u32Size,HI_U32 u32Uid);





/*****************************************************************************
 Prototype    : VB_AddBlkToPool
 Description  : add a buffer block to a specified pool
 Input        : u32PoolId      : ID of a pool
                u32PhysAddr    : physcial address of the buffer
                u32Uid         : ID of user getting the buffer
 Output       : 
 Return Value : VB_BLKHANDLE   : 
                success : Not VB_INVALID_HANDLE 
                failure : VB_INVALID_HANDLE 
*****************************************************************************/
extern HI_S32 VB_GetPoolInfo(VB_POOL_INFO_S *pstInfo);




extern HI_S32 VB_DestroyPool(HI_U32 u32PoolId);

/*****************************************************************************
 Prototype    : VB_GetPoolId
 Description  : Get the first one in common pools in which block size is greater
                than  'u32BlkSize'
 Input        : 
 Output       : 
 Return Value : HI_S32 
                success : 0
                failure : Not 0
*****************************************************************************/
extern HI_S32 VB_GetPoolId(HI_U32 u32BlkSize,HI_U32 *pu32PoolId,HI_CHAR *pcMmzName,HI_S32 s32Owner);

/*****************************************************************************
 Prototype    : VB_GetBlkByPoolId
 Description  : Get a buffer block from a specified pool
 Input        : u32PoolId      : ID of a pool
                u32Uid         : ID of user getting the buffer
 Output       : 
 Return Value : VB_BLKHANDLE   : 
                success : Not VB_INVALID_HANDLE 
                failure : VB_INVALID_HANDLE 
*****************************************************************************/
extern VB_BLKHANDLE VB_GetBlkByPoolId(HI_U32 u32PoolId,HI_U32 u32Uid);





/*****************************************************************************
 Prototype    : VB_GetBlkBySize
 Description  : Get a block in the first fit common pool
 Input        : u32BlkSize : size of buffer block
                u32Uid     : ID of user who will use the buffer
 Output       : NONE
 Return Value : VB_BLKHANDLE
                success : Not VB_INVALID_HANDLE 
                failure : VB_INVALID_HANDLE     
*****************************************************************************/
extern VB_BLKHANDLE VB_GetBlkBySize(HI_U32 u32BlkSize,HI_U32 u32Uid,HI_CHAR *pcMmzName);


/*****************************************************************************
 Prototype    : VB_GetBlkBySizeAndModule
 Description  : Get a block in the module common pool
 Input        : u32BlkSize : size of buffer block
                u32Uid     : ID of user who build the buffer
 Output       : NONE
 Return Value : VB_BLKHANDLE
                success : Not VB_INVALID_HANDLE 
                failure : VB_INVALID_HANDLE     
*****************************************************************************/
extern VB_BLKHANDLE VB_GetBlkBySizeAndModule(HI_U32 u32BlkSize, HI_U32 u32Uid, HI_CHAR *pcMmzName);



/*****************************************************************************
 Prototype    : VB_PutBlk
 Description  : forcebly free the buffer even if it is used by more than one
                user. Put a block back to free list, reset all couter for all users.
 Input        : u32PoolId      : ID of pool in which the buffer is.
                u32PhysAddr    : physical address of the buffer
 Output       : 
 Return Value : HI_S32
                success : 0
                failure : Not 0
*****************************************************************************/
extern HI_S32 VB_PutBlk(HI_U32 u32PoolId,HI_U32 u32PhysAddr);


/*****************************************************************************
 Prototype    : VB_Phy2Handle
 Description  : convert physical address to handle
 Input        : 
 Output       : 
 Return Value : HI_U32
******************************************************************************/
extern VB_BLKHANDLE VB_Phy2Handle(HI_U32 u32PhyAddr);

/*****************************************************************************
 Prototype    : VB_Handle2Phys
 Description  : convert handle to physical address of a buffer
 Input        : 
 Output       : 
 Return Value : HI_U32
******************************************************************************/
extern HI_U32 VB_Handle2Phys(VB_BLKHANDLE Handle);


/*****************************************************************************
 Prototype    : VB_Handle2Kern
 Description  : convert handle to kernel virtual address of a buffer
 Input        : Handle     : hanle of a buffer
 Output       :     
 Return Value : HI_U32
*****************************************************************************/
extern HI_U32 VB_Handle2Kern(VB_BLKHANDLE Handle);


/*****************************************************************************
 Prototype    : VB_Handle2PoolId
 Description  : convert Handle to pool ID
 Input        : VB_BLKHANDLE   : handle of the buffer
 Output       :
 Return Value : HI_U32
*****************************************************************************/
extern HI_U32 VB_Handle2PoolId(VB_BLKHANDLE Handle);

/*****************************************************************************
 Prototype    : VB_Handle2BlkId
 Description  : convert Handle to pool ID
 Input        : VB_BLKHANDLE   : handle of the buffer
 Output       :
 Return Value : HI_U32
*****************************************************************************/
extern HI_U32 VB_Handle2BlkId(VB_BLKHANDLE Handle);

/*****************************************************************************
 Prototype    : VB_UserAdd
 Description  : increase one to reference counter of  a buffer
 Input        : u32PoolId      : ID of pool the buffer is in
                u32PhysAddr    : physcial address of the buffer
                u32Uid         : ID of the user
 Output       :
 Return Value : HI_S32
                success    : 0
                failure    : Not 0
*****************************************************************************/
extern HI_S32 VB_UserAdd(HI_U32 u32PoolId,HI_U32 u32PhyAddr, HI_U32 u32Uid);


/*****************************************************************************
 Prototype    : VB_UserSub
 Description  : decrease one to reference counter of a buffer
 Input        : u32PoolId      : ID of pool the buffer is in
                u32PhysAddr    : physcial address of the buffer
                u32Uid         : ID of the user
 Output       : 
 Return Value : HI_S32
*****************************************************************************/
extern HI_S32 VB_UserSub(HI_U32 u32PoolId,HI_U32 u32PhyAddr, HI_U32 u32Uid);


/*****************************************************************************
 Prototype    : VB_InquireUserCnt
 Description  : Inquire how many users are using the buffer
 Input        : Handle     : hanle of a buffer
 Output       : 
 Return Value : HI_U32
******************************************************************************/
extern HI_U32 VB_InquireUserCnt(VB_BLKHANDLE Handle);

extern HI_U32 VB_InquireOneUserCnt(VB_BLKHANDLE Handle, HI_U32 u32Uid);

/*****************************************************************************
 Prototype    : VB_InquireBlkCnt
 Description  : Inquire how many blocks the user is using
 Input        : u32Uid
                bIsCommPool
 Output       : 
 Return Value : HI_U32
*****************************************************************************/
extern HI_U32 VB_InquireBlkCnt(HI_U32 u32Uid, HI_BOOL bIsCommPool);

/*****************************************************************************
 Prototype    : VB_InquirePool
 Description  : Inquire the statistic of a pool
 Input        : u32PoolId     : ID of pool the buffer is in
 Output       : pstPoolStatus : pool status
 Return Value : HI_U32
******************************************************************************/
extern HI_S32 VB_InquirePool(HI_U32 u32PoolId, VB_POOL_STATUS_S *pstPoolStatus);

/*****************************************************************************
 Prototype    : VB_InquirePoolUserCnt
 Description  : Inquire how many blocks the user used in this pool
 Input        : u32PoolId     : ID of pool the buffer is in
 				u32Uid		  : ID of the user
 Output       : pu32Cnt		  : Count of vb used by user
 Return Value : HI_U32
******************************************************************************/
extern HI_S32 VB_InquirePoolUserCnt(HI_U32 u32PoolId, HI_U32 u32Uid, HI_U32 *pu32Cnt);

/*****************************************************************************
 Prototype    : VB_IsBlkValid
 Description  : Check if the address is valid
 Input        : u32PoolId
                u32PhysAddr
 Output       : 
 Return Value : HI_BOOL
*****************************************************************************/
extern HI_BOOL VB_IsBlkValid(HI_U32 u32PoolId, HI_U32 u32PhysAddr);


/*****************************************************************************
 Prototype    : VB_Init
 Description  : Initialize video buffer modules
 Input        : pArg       : unused now
 Output       :
 Return Value : HI_S32
                success    : 0
                failure    : Not 0
*****************************************************************************/
extern HI_S32 VB_Init(HI_VOID *);


/*****************************************************************************
 Prototype    : VB_Exit
 Description  : cleanup video buffer module
 Input        :
 Output       : 
 Return Value :HI_S32
               success    : 0
               failure    : Not 0 , we never reture failure in fact.
*****************************************************************************/
extern HI_S32 VB_Exit(HI_VOID);

#endif /* __VB_EXT_H__ */

