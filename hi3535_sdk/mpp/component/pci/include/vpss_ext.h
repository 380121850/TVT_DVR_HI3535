

/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : hi_sys.h
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 
  Description   :
  History       :
  1.Date        : 
    Author      : 
    Modification: 
    
 

******************************************************************************/
#include "hi_type.h"
#include "mod_ext.h"

#include "hi_comm_vpss.h"
#include "mkp_sys.h"
#include "mkp_vpss.h"

#ifndef __VPSS_EXT_H__
#define __VPSS_EXT_H__


typedef struct HI_VPSS_PIC_INFO_S
{   
    VIDEO_FRAME_INFO_S stVideoFrame;
    MOD_ID_E        enModId;
    //VPSS_PRESCALE_E         enVpssPresclType;               /*预缩放类型*/    
    HI_BOOL bFlashed;               /* Flashed Video frame or not. */
    HI_BOOL bBlock;               /* Flashed Video frame or not. */
}VPSS_PIC_INFO_S;


typedef struct HI_VPSS_QUERY_INFO_S
{
    VPSS_PIC_INFO_S* pstSrcPicInfo;     /*源图像图像信息*/
    VPSS_PIC_INFO_S* pstOldPicInfo;     /*backup图像信息*/
    HI_BOOL          bScaleCap;         /*是否具有缩放能力*/   
    HI_BOOL          bTransCap;         /*是否具有30帧到60帧的能力*/
    HI_BOOL          bMalloc;           /*是否需要分配帧存*/
}VPSS_QUERY_INFO_S;

typedef struct HI_VPSS_INST_INFO_S
{   
    HI_BOOL          bNew;               /*是否使用新图像query*/
    HI_BOOL          bVpss;              /*是否做vpss*/
    HI_BOOL          bDouble;            /*是否30-60F*/
    HI_BOOL          bUpdate;            /*是否更新backup区，只有在NEW+UNDO的情况下才依赖此标志*/
    COMPRESS_MODE_E enCompressMode;      /*是否压缩*/
    VPSS_PIC_INFO_S  astDestPicInfo[2];  /*目标图像信息,0:用于顶插底的帧存 1:用于底插顶的帧存*/
}VPSS_INST_INFO_S;

typedef struct HI_VPSS_HIST_INFO_S
{
   HI_U32          u32PhyAddr;
   HI_VOID         *pVirAddr;
   HI_U32          u32Length;
   HI_U32          u32PoolId;
   HI_BOOL         bQuarter;
   
}VPSS_HIST_INFO_S;


typedef struct HI_VPSS_SEND_INFO_S
{
    HI_BOOL           bSuc;                  /*是否成功完成*/
    VPSS_GRP          VpssGrp;
    VPSS_CHN          VpssChn;
    VPSS_PIC_INFO_S   *pstDestPicInfo[2];    /*完成VPSS后的图像信息.0:顶插底的图像 1:底插顶的图像*/
    VPSS_HIST_INFO_S  *pstHistInfo;          /*统计的直方图信息*/
}VPSS_SEND_INFO_S;


typedef struct HI_VPSS_REGISTER_INFO_S
{
    MOD_ID_E    enModId;
    HI_S32      (*pVpssQuery)(HI_S32 s32DevId, HI_S32 s32ChnId, VPSS_QUERY_INFO_S  *pstQueryInfo, VPSS_INST_INFO_S *pstInstInfo);
    HI_S32      (*pVpssSend)(HI_S32 s32DevId, HI_S32 s32ChnId, VPSS_SEND_INFO_S  *pstSendInfo);
    HI_S32      (*pResetCallBack)(HI_S32 s32DevId, HI_S32 s32ChnId, HI_VOID *pvData);
 
}VPSS_REGISTER_INFO_S;


typedef struct hiVPSS_EXPORT_FUNC_S
{
    HI_S32  (*pfnVpssRegister)(VPSS_REGISTER_INFO_S *pstInfo);
    HI_S32  (*pfnVpssUnRegister)(MOD_ID_E enModId);
    
}VPSS_EXPORT_FUNC_S;

#define CKFN_VPSS_ENTRY()  CHECK_FUNC_ENTRY(HI_ID_VPSS)

#define CKFN_VPSS_Register() \
    (NULL != FUNC_ENTRY(VPSS_EXPORT_FUNC_S, HI_ID_VPSS)->pfnVpssRegister)
#define CALL_VPSS_Register(pstInfo) \
    FUNC_ENTRY(VPSS_EXPORT_FUNC_S, HI_ID_VPSS)->pfnVpssRegister(pstInfo)

#define CKFN_VPSS_UnRegister() \
    (NULL != FUNC_ENTRY(VPSS_EXPORT_FUNC_S, HI_ID_VPSS)->pfnVpssUnRegister)
#define CALL_VPSS_UnRegister(enModId) \
    FUNC_ENTRY(VPSS_EXPORT_FUNC_S, HI_ID_VPSS)->pfnVpssUnRegister(enModId)

#endif /* __VPSS_EXT_H__ */




