/******************************************************************************

  Copyright (C), 2013-2023, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : vgs_ext.h
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2013/04/23
  Description   :
  History       :
  1.Date        : 2013/04/23
    Author      : z00183560
    Modification: Created file
******************************************************************************/

#ifndef __VGS_EXT_H__
#define __VGS_EXT_H__

#include "hi_comm_video.h"
#include "hi_comm_vgs.h"

#include "hi_errno.h"
#include "dci.h"
#define HI_TRACE_VGS(level, fmt...)	\
do{\
	HI_TRACE(level, HI_ID_VGS, "[%s]: %d,", __FUNCTION__, __LINE__);\
	HI_TRACE(level, HI_ID_VGS, ##fmt);\
} while(0);

#define VGS_INVALD_HANDLE          (-1UL) /*invalid job handle*/

#define VGS_MAX_JOB_NUM             400  /*max job number*/
#define VGS_MIN_JOB_NUM             20  /*min job number*/

#define VGS_MAX_TASK_NUM            800  /*max task number*/
#define VGS_MIN_TASK_NUM            20  /*min task number*/

#define VGS_MAX_NODE_NUM            800  /*max node number*/
#define VGS_MIN_NODE_NUM            20  /*min node number*/

#define VGS_MAX_WEIGHT_THRESHOLD    100  /*max weight threshold*/
#define VGS_MIN_WEIGHT_THRESHOLD    1  /*min weight threshold*/


#define MAX_VGS_COVER               4
#define MAX_VGS_OSD                 8

/*Task的操作类型*/
typedef enum hiVGS_JOB_TYPE_E
{
    VGS_JOB_TYPE_BYPASS = 0,        /*BYPASS job,can only contain bypass task*/
    VGS_JOB_TYPE_NORMAL = 1,         /*normal job,can contain any task except bypass task and lumastat task*/
    VGS_JOB_TYPE_BUTT
}VGS_JOB_TYPE_E;


/*task完成状态*/
typedef enum hiVGS_TASK_FNSH_STAT_E
{
    VGS_TASK_FNSH_STAT_OK = 1,   /*task已正确完成*/
    VGS_TASK_FNSH_STAT_FAIL = 2, /*task执行出现异常或未完成*/
    VGS_TASK_FNSH_STAT_CANCELED= 3, /*被cancel掉*/
    VGS_TASK_FNSH_STAT_BUTT
}VGS_TASK_FNSH_STAT_E;

/*VGS操作数据信息结构，包括输入、输出图像信息及调用者，回调函数信息。
  VGS任务完成后，每个操作以回调函数形式通知用户，该结构作为回调函数参数
  返回该操作对应的数据信息
*/
typedef struct hiVGS_TASK_DATA_S
{
    VIDEO_FRAME_INFO_S      stImgIn;        /* input picture */
    VIDEO_FRAME_INFO_S      stImgOut;       /* output picture */
    HI_U32                  au32privateData[4];    /* task's private data */
    void                    (*pCallBack)(MOD_ID_E enCallModId,HI_U32 u32CallDevId,const struct hiVGS_TASK_DATA_S *pTask); /* callback */
    MOD_ID_E	            enCallModId;    /* module ID */
    HI_U32                  u32CallChnId;    /*chnnal ID */
    VGS_TASK_FNSH_STAT_E    enFinishStat;     /* output param:task finish status(ok or nok)*/
    HI_U32                  reserved;       /* save current picture's state while debug */   
}VGS_TASK_DATA_S;

/*VGS任务的优先级级别*/
typedef enum hiVGS_JOB_PRI_E
{
    VGS_JOB_PRI_HIGH = 0,      /*高优先级*/
    VGS_JOB_PRI_NORMAL =1,     /*中优先级*/
    VGS_JOB_PRI_LOW =2,        /*低优先级*/
    VGS_JOB_PRI_BUTT
}VGS_JOB_PRI_E;


/*VGS cancel任务的输出值*/
typedef struct hiVGS_CANCEL_STAT_S
{
    HI_U32  u32JobsCanceled;    //成功cancel多少个job
    HI_U32  u32JobsLeft;        //还剩余对少个job在VGS里面
}VGS_CANCEL_STAT_S;


/*job完成状态*/
typedef enum hiVGS_JOB_FNSH_STAT_E
{
    VGS_JOB_FNSH_STAT_OK = 1,   /*JOB已正确完成(表示其下的所有task已正常完成)*/
    VGS_JOB_FNSH_STAT_FAIL = 2, /*JOB执行出现异常或未完成*/
    VGS_JOB_FNSH_STAT_CANCELED = 3, /*JOB执行出现异常或未完成*/
    VGS_JOB_FNSH_STAT_BUTT
}VGS_JOB_FNSH_STAT_E;


/*VGS任务信息结构，一个vgs job完成后通知用户时返回给用户的信息*/
typedef struct hiVGS_JOB_DATA_S
{
    HI_U32                  au32PrivateData[4];    /* job级的用户私有数据 */
    VGS_JOB_FNSH_STAT_E     enJobFinishStat;        /* output param:job finish status(ok or nok)*/
    VGS_JOB_TYPE_E          enJobType;
    void                    (*pJobCallBack)(MOD_ID_E enCallModId,HI_U32 u32CallDevId,struct hiVGS_JOB_DATA_S *pJobData); /* callback */
}VGS_JOB_DATA_S;


/*指定OSD叠加操作的前景位图的信息 */
typedef struct hiVGS_OSD_S
{
    /* 位图首地址 */
    HI_U32 u32PhyAddr;
    /* 颜色格式 */
    PIXEL_FORMAT_E enPixelFormat;

    /* 位图跨度 */
    HI_U32 u32Stride;

    /*如果是ARGB1555格式 可指定alpha通道的是否扩展及Alpha0、Alpha1值 */
    HI_BOOL bAlphaExt1555; /* 是否使能1555的Alpha扩展 */
    HI_U8 u8Alpha0;        /* 扩展alpha通道值为0的像素的alpha值为u8Alpha0*/
    HI_U8 u8Alpha1;        /* 扩展alpha通道值为1的像素的alpha值为u8Alpha1*/
} VGS_OSD_S;

/*OSD叠加的操作结构*/
typedef struct hiVGS_OSD_OPT_S
{
    HI_BOOL         bOsdEn;
    /*全局Alpha值*/
    HI_U8           u8GlobalAlpha;
    VGS_OSD_S       stVgsOsd;
    RECT_S          stOsdRect;
} VGS_OSD_OPT_S;

typedef struct hiVGS_LUMAINFO_OPT_S
{
    RECT_S      stRect;             /*the regions to get lumainfo*/
    HI_U32 *    pu32VirtAddrForResult;
    HI_U32      u32PhysAddrForResult;
}VGS_LUMASTAT_OPT_S;


/*Define 4 video frame*/
typedef enum HI_VGS_BORDER_WORK_E
{
    VGS_BORDER_WORK_LEFT     =  0,
    VGS_BORDER_WORK_RIGHT    =  1,
    VGS_BORDER_WORK_BOTTOM   =  2,
    VGS_BORDER_WORK_TOP      =  3,
    VGS_BORDER_WORK_BUTT
}VGS_BORDER_WORK_E;


/*Define attributes of video frame*/
typedef struct HI_VGS_FRAME_OPT_S
{
    RECT_S  stRect;
    HI_U32  u32Width; /*Width of 4 frames,0:L,1:R,2:B,3:T*/
    HI_U32  u32Color; /*Color of 4 frames,R/G/B*/
}VGS_FRAME_OPT_S;

/*Define attributes of video frame*/
typedef struct HI_VGS_BORDER_OPT_S
{
    HI_U32  u32Width[VGS_BORDER_WORK_BUTT]; /*Width of 4 frames,0:L,1:R,2:B,3:T*/
    HI_U32  u32Color; /*Color of 4 frames,R/G/B*/
}VGS_BORDER_OPT_S;


typedef struct hiVGS_ROTATE_OPT_S
{
    ROTATE_E enRotateAngle;

}VGS_ROTATE_OPT_S;

typedef LDC_ATTR_S  VGS_LDC_OPT_S;

typedef struct hiVGS_COVER_OPT_S
{
    HI_BOOL     bCoverEn;
    RECT_S      stDstRect;
    HI_U32      u32CoverData;
}VGS_COVER_OPT_S;


typedef struct hiVGS_HSHARPEN_OPT_S
{
    HI_U32 u32LumaGain;           /*Luma gain of sharp function*/   
}VGS_HSHARPEN_OPT_S;

typedef struct hiVGS_CROP_OPT_S
{
    RECT_S stDestRect;
}VGS_CROP_OPT_S;


typedef struct hiVGS_DCI_OPT_S
{
    HI_S32      s32Strength;    /*0-64*/
    HI_S32      s32CurFrmNum;    
    HI_BOOL     bFullRange;     /**/
    HI_BOOL     bResetDci;
    HI_BOOL     bEnSceneChange;
    HI_U32      u32HistPhyAddr;
    HI_S32 *    ps32HistVirtAddr;
    HI_U32      u32HistSize;
    DCIParam*   pstDciParam;
}VGS_DCI_OPT_S;


typedef struct hiVGS_PARAM_S
{
     /* 是否强制滤波*/
     /* 缩放策略*/
}VGS_PARAM_S;


typedef struct hiVGS_ONLINE_OPT_S
{
    HI_BOOL                 bCrop;              /*if enable crop*/
    VGS_CROP_OPT_S          stCropOpt;
    HI_BOOL                 bCover;             /*if enable cover*/
    VGS_COVER_OPT_S         stCoverOpt[MAX_VGS_COVER];   
    HI_BOOL                 bOsd;               /*if enable osd*/
    VGS_OSD_OPT_S           stOsdOpt[MAX_VGS_OSD];  
    HI_BOOL                 bHSharpen;          /*if enable sharpen*/
    VGS_HSHARPEN_OPT_S      stHSharpenOpt; 
    HI_BOOL                 bBorder;            /*if enable Border*/
    VGS_BORDER_OPT_S        stBorderOpt; 
    HI_BOOL                 bMirror;            /*if enable mirror*/
    HI_BOOL                 bFlip;              /*if enable flip*/
    VGS_PARAM_S             stVgsParams;
    HI_BOOL                 bDci;               /*if enable dci*/
    VGS_DCI_OPT_S           stDciOpt;
    HI_BOOL                 bForceHFilt;        /*是否强制水平滤波，当输入输出图像
                                                        水平方向大小相同时可设置强制水平滤波*/
    HI_BOOL                 bForceVFilt;        /*是否强制垂直滤波，当输入输出图像
                                                        垂直方向大小相同时可设置强制垂直滤波*/	
    HI_BOOL                 bDeflicker          /*是否抗闪烁*/;
}VGS_ONLINE_OPT_S;

typedef HI_S32 	    FN_VGS_Init(HI_VOID* pVrgs);

typedef HI_VOID     FN_VGS_Exit(HI_VOID);

typedef HI_S32      FN_VGS_BeginJob(VGS_HANDLE *pVgsHanle,VGS_JOB_PRI_E enPriLevel,
                                    MOD_ID_E	enCallModId,HI_U32 u32CallChnId,VGS_JOB_DATA_S *pJobData);

typedef HI_S32      FN_VGS_EndJob(VGS_HANDLE VgsHanle);


typedef HI_S32      FN_VGS_CancelJob(VGS_HANDLE hHandle);

typedef HI_S32      FN_VGS_CancelJobByModDev(MOD_ID_E	enCallModId,HI_U32 u32CallDevId,VGS_CANCEL_STAT_S*  pstVgsCancelStat);

typedef HI_S32      FN_VGS_AddFrameTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask,VGS_FRAME_OPT_S pstVgsFrameOpt);

typedef HI_S32      FN_VGS_AddCoverTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask,VGS_COVER_OPT_S* pstCoverOpt);

typedef HI_S32      FN_VGS_AddRotateTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask,VGS_ROTATE_OPT_S* pstRotateOpt);

typedef HI_S32      FN_VGS_AddOSDTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask,VGS_OSD_OPT_S* pstOsdOpt);

typedef HI_S32      FN_VGS_AddBypassTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask);

typedef HI_S32      FN_VGS_AddGetLumaStatTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask,VGS_LUMASTAT_OPT_S* pstLumaInfoOpt);

typedef HI_S32      FN_VGS_AddOnlineTask(VGS_HANDLE VgsHanle,VGS_TASK_DATA_S *pTask,VGS_ONLINE_OPT_S* pstOnlineOpt); 

typedef HI_S32      FN_VGS_AddLDCTask(VGS_HANDLE hHandle, VGS_TASK_DATA_S *pTask, VGS_LDC_OPT_S *pstLdcOpt);

/*only for test*/
typedef HI_VOID     FN_VGS_GetMaxJobNum(HI_S32* s32MaxJobNum);
typedef HI_VOID     FN_VGS_GetMaxTaskNum(HI_S32* s32MaxTaskNum);


typedef struct hiVGS_EXPORT_FUNC_S
{
    FN_VGS_BeginJob         *pfnVgsBeginJob;
    FN_VGS_CancelJob        *pfnVgsCancelJob;
    FN_VGS_CancelJobByModDev    *pfnVgsCancelJobByModDev;
    FN_VGS_EndJob           *pfnVgsEndJob;
    FN_VGS_AddFrameTask     *pfnVgsAddFrameTask;
    FN_VGS_AddCoverTask     *pfnVgsAddCoverTask;
    FN_VGS_AddRotateTask    *pfnVgsAddRotateTask;
    FN_VGS_AddOSDTask       *pfnDVgsAddOSDTask;
    FN_VGS_AddBypassTask    *pfnVgsAddBypassTask;
    FN_VGS_AddGetLumaStatTask   *pfnVgsGetLumaStatTask;
    FN_VGS_AddOnlineTask    *pfnVgsAddOnlineTask;
    FN_VGS_AddLDCTask       *pfnVgsAddLDCTask;

    /*only for test*/
    FN_VGS_GetMaxJobNum     *pfnVgsGetMaxJobNum;
    FN_VGS_GetMaxTaskNum    *pfnVgsGetMaxTaskNum;
}VGS_EXPORT_FUNC_S;



#endif /* __VGS_H__ */

