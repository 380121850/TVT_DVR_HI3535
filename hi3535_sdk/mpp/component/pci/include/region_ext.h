/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

 ******************************************************************************
  �� �� ��   : region_ext.h
  �� �� ��   : ����
  ��    ��   : l64467
  ��������   : 2010��12��18��
  ����޸�   :
  ��������   : 
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��12��18��
    ��    ��   : l64467
    �޸�����   : �����ļ�
  
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


/*���֧�ֵ��������ظ�ʽ*/
#define PIXEL_FORMAT_NUM_MAX 3

typedef struct hiRGN_COMM_S
{
    HI_BOOL bShow;           /* �����Ƿ���ʾ*/
    POINT_S stPoint;         /*������ʼλ��*/
    SIZE_S  stSize;          /*������ʼ���*/
    HI_U32  u32Layer;        /*������*/
    HI_U32  u32BgColor;      /*���򱳾�ɫ*/
    HI_U32  u32GlobalAlpha;  /*����ȫ��ALPHA*/
    HI_U32  u32FgAlpha;      /*����ǰ��ALPHA*/
    HI_U32  u32BgAlpha;      /*���򱳾�ALPHA*/
    HI_U32  u32PhyAddr;      /*������ռ���ڴ�������ַ*/
	HI_U32  u32VirtAddr;     /*����ռ���ڴ�������ַ*/
	HI_U32  u32Stride;       /*���������ݵ� Stride*/
    PIXEL_FORMAT_E      enPixelFmt;         /*�������ظ�ʽ*/
    VIDEO_FIELD_E       enAttachField;      /*�����ŵ�֡����Ϣ*/

    OVERLAY_QP_INFO_S   stQpInfo;           /*QP��Ϣ*/
    
    OVERLAY_INVERT_COLOR_S stInvColInfo;    /*��ɫ��Ϣ*/
    
}RGN_COMM_S;

typedef struct hiRGN_INFO_S
{
    HI_U32 u32Num;           /* �������*/
    HI_BOOL bModify;         /*�Ƿ��Ѿ��޸�*/
    RGN_COMM_S **ppstRgnComm;/* ���򹫹���Ϣָ������ĵ�ַ*/ 
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

    /* ��ʼ��X���ض�����Ŀ */
    HI_U32 u32StartXAlign;

    /* ��ʼ��Y���ض�����Ŀ */
    HI_U32 u32StartYAlign;
    
}RGN_CAPACITY_POINT_S;

typedef struct hiRGN_CAPACITY_SIZE_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;

    SIZE_S stSizeMin;
    SIZE_S stSizeMax;

    /* ����������Ŀ */
    HI_U32 u32WidthAlign;

    /* ����߶�����Ŀ */
    HI_U32 u32HeightAlign;

    /*����������*/
    HI_U32 u32MaxArea;
}RGN_CAPACITY_SIZE_S;

typedef struct hiRGN_CAPACITY_PIXLFMT_S
{
    HI_BOOL bComm;
    HI_BOOL bSptReSet;

    /* ֧�����ظ�ʽ���� */
    HI_U32 u32PixlFmtNum;
    /* ֧�����ظ�ʽ���������ͣ�----------��ؼ����:8.1�����ͨ�����и�ʽ�Ƿ���ͬ */
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
    /* �Ƿ���Ҫ���� */
    HI_BOOL bNeedSort;

    /* ����ؼ��� */
    RGN_SORT_KEY_E enRgnSortKey;

    /* ����˳�� TRUE:��С����FALSE:�Ӵ�С */
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

    /* ���ʹ���˷�ɫ���ܣ���������ʼ��X���ض�����Ŀ */
    HI_U32 u32StartXAlign;
    /* ���ʹ���˷�ɫ���ܣ���������ʼ��Y���ض�����Ŀ */
    HI_U32 u32StartYAlign;
    /* ���ʹ���˷�ɫ���ܣ��������ȶ�����Ŀ */
    HI_U32 u32WidthAlign;
    /* ���ʹ���˷�ɫ���ܣ�������߶ȶ�����Ŀ */
    HI_U32 u32HeightAlign;
    
}RGN_CAPACITY_INVCOLOR_S;

typedef struct hiRGN_CAPACITY_S
{ 
    /*������*/
    RGN_CAPACITY_LAYER_S stLayer;

    /*������ʼλ��*/
    RGN_CAPACITY_POINT_S stPoint;

    /*�����С*/
    RGN_CAPACITY_SIZE_S  stSize;

    /*���ظ�ʽ */
    HI_BOOL bSptPixlFmt;
    RGN_CAPACITY_PIXLFMT_S stPixlFmt;

    /*alphaֵ */
    /* ǰ�� alpha */
    HI_BOOL bSptFgAlpha;
    RGN_CAPACITY_ALPHA_S stFgAlpha;

    /*���� alpha */
    HI_BOOL bSptBgAlpha;
    RGN_CAPACITY_ALPHA_S stBgAlpha;

    /*ȫ�� alpha */
    HI_BOOL bSptGlobalAlpha;
    RGN_CAPACITY_ALPHA_S stGlobalAlpha;

    /*����ɫ */
    HI_BOOL bSptBgClr;
    RGN_CAPACITY_BGCLR_S stBgClr;

    /*���� */
    HI_BOOL bSptChnSort;
    RGN_CAPACITY_SORT_S stChnSort;

    /*QP������*/
    HI_BOOL bSptQp;
    RGN_CAPACITY_QP_S stQp;
    
    /*��ɫ*/
    HI_BOOL bSptInvColoar;
    RGN_CAPACITY_INVCOLOR_S stInvColor;

    /*�Ƿ�֧��λͼ */
    HI_BOOL bSptBitmap;

    /*�Ƿ�֧���ص� */
    HI_BOOL bsptOverlap;

    /*�п��ڴ����*/
    HI_U32 u32Stride;

    /*ÿ��ͨ���������������*/
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



