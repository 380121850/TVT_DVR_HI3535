/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : region_ext.h
  版 本 号   : 初稿
  作    者   : l64467
  生成日期   : 2010年12月18日
  最近修改   :
  功能描述   : 
  函数列表   :
  修改历史   :
  1.日    期   : 2010年12月18日
    作    者   : l64467
    修改内容   : 创建文件
  
******************************************************************************/
#ifndef __REGION_EXT_H__
#define __REGION_EXT_H__

#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_region.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */


/*最大支持的区域像素格式*/
#define PIXEL_FORMAT_NUM_MAX 3

typedef struct hiRGN_COMM_S
{
    HI_BOOL bShow;           /* 区域是否显示*/
    POINT_S stPoint;         /*区域起始位置*/
    SIZE_S  stSize;          /*区域起始宽高*/
    HI_U32  u32Layer;        /*区域层次*/
    HI_U32  u32BgColor;      /*区域背景色*/
    HI_U32  u32GlobalAlpha;  /*区域全景ALPHA*/
    HI_U32  u32FgAlpha;      /*区域前景ALPHA*/
    HI_U32  u32BgAlpha;      /*区域背景ALPHA*/
    HI_U32  u32PhyAddr;      /*区域所占有内存的物理地址*/
	HI_U32  u32VirtAddr;     /*区域占有内存的虚拟地址*/
	HI_U32  u32Stride;       /*区域内数据的 Stride*/
    PIXEL_FORMAT_E      enPixelFmt;         /*区域像素格式*/
    VIDEO_FIELD_E       enAttachField;      /*区域附着的帧场信息*/

    OVERLAY_QP_INFO_S   stQpInfo;           /*QP信息*/
    
    OVERLAY_INVERT_COLOR_S stInvColInfo;    /*反色信息*/
    
}RGN_COMM_S;

typedef struct hiRGN_INFO_S
{
    HI_U32 u32Num;           /* 区域个数*/
    HI_BOOL bModify;         /*是否已经修改*/
    RGN_COMM_S **ppstRgnComm;/* 区域公共信息指针数组的地址*/ 
}RGN_INFO_S;

typedef struct hiRGN_REGISTER_INFO_S
{
    MOD_ID_E    enModId;
    HI_U32      u32MaxDevCnt;   /* If no dev id, should set it 1 */
    HI_U32      u32MaxChnCnt;
} RGN_REGISTER_INFO_S;

typedef struct hiRGN_CAPACITY_LAYER_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;
    
    /* Region Layer [min,max] */
    HI_U32 u32LayerMin;
    HI_U32 u32LayerMax;
}RGN_CAPACITY_LAYER_S;

typedef struct hiRGN_CAPACITY_POINT_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;

    POINT_S stPointMin;
    POINT_S stPointMax;

    /* 起始点X像素对齐数目 */
    HI_U32 u32StartXAlign;

    /* 起始点Y像素对齐数目 */
    HI_U32 u32StartYAlign;
    
}RGN_CAPACITY_POINT_S;

typedef struct hiRGN_CAPACITY_SIZE_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;

    SIZE_S stSizeMin;
    SIZE_S stSizeMax;

    /* 区域宽对齐数目 */
    HI_U32 u32WidthAlign;

    /* 区域高对齐数目 */
    HI_U32 u32HeightAlign;

    /*区域最大面积*/
    HI_U32 u32MaxArea;
}RGN_CAPACITY_SIZE_S;

typedef struct hiRGN_CAPACITY_PIXLFMT_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;

    /* 支持像素格式个数 */
    HI_U32 u32PixlFmtNum;
    /* 支持像素格式的所有类型，----------相关检查项:8.1，检查通道所有格式是否相同 */
    PIXEL_FORMAT_E aenPixlFmt[PIXEL_FORMAT_NUM_MAX];
}RGN_CAPACITY_PIXLFMT_S;

typedef struct hiRGN_CAPACITY_ALPHA_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;
    
    HI_U32 u32AlphaMax;
    HI_U32 u32AlphaMin;
}RGN_CAPACITY_ALPHA_S;
 
typedef struct hiRGN_CAPACITY_BGCLR_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;
}RGN_CAPACITY_BGCLR_S;

typedef enum hiRGN_SORT_KEY_E
{
    RGN_SORT_BY_LAYER = 0,
    RGN_SORT_BY_POSITION,
    RGN_SRT_BUTT
}RGN_SORT_KEY_E;

typedef struct hiRGN_CAPACITY_SORT_S
{
    /* 是否需要排序 */
    HI_BOOL bNeedSort;

    /* 排序关键字 */
    RGN_SORT_KEY_E enRgnSortKey;

    /* 排序顺序 TRUE:从小到大；FALSE:从大到小 */
    HI_BOOL bSmallToBig;
}RGN_CAPACITY_SORT_S;

typedef struct hiRGN_CAPACITY_QP_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;

    HI_S32 s32QpAbsMin;
    HI_S32 s32QpAbsMax;

    HI_S32 s32QpRelMin;
    HI_S32 s32QpRelMax;
    
}RGN_CAPACITY_QP_S;

typedef struct hiRGN_CAPACITY_INVCOLOR_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;
    
    RGN_CAPACITY_SIZE_S stSizeCap;
    
    HI_U32 u32LumMin;
    HI_U32 u32LumMax;

    INVERT_COLOR_MODE_E enInvModMin;
    INVERT_COLOR_MODE_E enInvModMax;

    /* 如果使用了反色功能，对区域起始点X像素对齐数目 */
    HI_U32 u32StartXAlign;
    /* 如果使用了反色功能，对区域起始点Y像素对齐数目 */
    HI_U32 u32StartYAlign;
    /* 如果使用了反色功能，对区域宽度对齐数目 */
    HI_U32 u32WidthAlign;
    /* 如果使用了反色功能，对区域高度对齐数目 */
    HI_U32 u32HeightAlign;
    
}RGN_CAPACITY_INVCOLOR_S;

typedef struct hiRGN_CAPACITY_S
{ 
    /*区域层次*/
    RGN_CAPACITY_LAYER_S stLayer;

    /*区域起始位置*/
    RGN_CAPACITY_POINT_S stPoint;

    /*区域大小*/
    RGN_CAPACITY_SIZE_S  stSize;

    /*像素格式 */
    HI_BOOL bSptPixlFmt;
    RGN_CAPACITY_PIXLFMT_S stPixlFmt;

    /*alpha值 */
    /* 前景 alpha */
    HI_BOOL bSptFgAlpha;
    RGN_CAPACITY_ALPHA_S stFgAlpha;

    /*背景 alpha */
    HI_BOOL bSptBgAlpha;
    RGN_CAPACITY_ALPHA_S stBgAlpha;

    /*全景 alpha */
    HI_BOOL bSptGlobalAlpha;
    RGN_CAPACITY_ALPHA_S stGlobalAlpha;

    /*背景色 */
    HI_BOOL bSptBgClr;
    RGN_CAPACITY_BGCLR_S stBgClr;

    /*排序 */
    HI_BOOL bSptChnSort;
    RGN_CAPACITY_SORT_S stChnSort;

    /*QP能力集*/
    HI_BOOL bSptQp;
    RGN_CAPACITY_QP_S stQp;
    
    /*反色*/
    HI_BOOL bSptInvColoar;
    RGN_CAPACITY_INVCOLOR_S stInvColor;

    /*是否支持位图 */
    HI_BOOL bSptBitmap;

    /*是否支持重叠 */
    HI_BOOL bsptOverlap;

    /*行宽内存对齐*/
    HI_U32 u32Stride;

    /*每个通道里区域的最大个数*/
    HI_U32 u32RgnNumInChn;
}RGN_CAPACITY_S;

typedef struct hiRGN_EXPORT_FUNC_S
{
    HI_S32 (*pfnRgnRegisterMod)(RGN_TYPE_E enType, const RGN_CAPACITY_S *pstCapacity, const RGN_REGISTER_INFO_S *pstRgtInfo);
    HI_S32 (*pfnUnRgnRegisterMod)(RGN_TYPE_E enType, MOD_ID_E enModId);
    
    HI_S32 (*pfnRgnGetRegion)(RGN_TYPE_E enType, const MPP_CHN_S *pstChn, RGN_INFO_S *pstRgnInfo);
    HI_S32 (*pfnRgnPutRegion)(RGN_TYPE_E enType, const MPP_CHN_S *pstChn);
    HI_S32 (*pfnRgnSetModifyFalse)(RGN_TYPE_E enType, const MPP_CHN_S *pstChn);
}RGN_EXPORT_FUNC_S;


#define CKFN_RGN() \
    (NULL != FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN))


#define CKFN_RGN_RegisterMod() \
    (NULL != FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnRegisterMod)
    

#define CALL_RGN_RegisterMod(enType, pstCapacity, pstRgtInfo) \
    FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnRegisterMod(enType, pstCapacity, pstRgtInfo)


#define CKFN_RGN_UnRegisterMod() \
    (NULL != FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnUnRgnRegisterMod)
    

#define CALL_RGN_UnRegisterMod(enType, enModId) \
    FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnUnRgnRegisterMod(enType, enModId)
    

#define CKFN_RGN_GetRegion() \
    (NULL != FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnGetRegion)
    

#define CALL_RGN_GetRegion(enType, pstChn, pstRgnInfo) \
    FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnGetRegion(enType, pstChn, pstRgnInfo)
    


#define CKFN_RGN_PutRegion() \
    (NULL != FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnPutRegion)
    
#define CALL_RGN_PutRegion(enType, pstChn) \
    FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnPutRegion(enType, pstChn)


#define CKFN_RGN_SetModifyFalse() \
    (NULL != FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnSetModifyFalse)
    
#define CALL_RGN_SetModifyFalse(enType, pstChn) \
    FUNC_ENTRY(RGN_EXPORT_FUNC_S, HI_ID_RGN)->pfnRgnSetModifyFalse(enType, pstChn)


#ifdef __cplusplus
    #if __cplusplus
    }
    #endif
#endif /* End of #ifdef __cplusplus */

#endif /* __REGION_EXT_H__ */



