/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : hi_config.h
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2005/5/30
  Last Modified :
  Description   : hi_config.h header file
  Function List :
  History       :
  1.Date        : 2005/5/30
    Author      : T41030
    Modification: Created file

******************************************************************************/

#ifndef __HI_CONFIG_H__
#define __HI_CONFIG_H__


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */


#if defined(TARGET_X86)

/* ��Makefile��
    ��PC��ģ���ʵ��
    ��PC��ģ��ʱ(linux), ��ֱ�ӷ��������ڴ��ַ, ����Ϊ����һ���ڴ�ķ�ʽ����ȡ��Ϣ��
*/

//#define PC_EMULATOR
/*#undefine PC_EMULATOR*/

/*�Ƿ�ʹ��BTRACE�ĵ��Կ�*/
/*#define USE_BTRACE*/
/*#undefine USE_BTRACE*/
#endif


/* ����ģʽ */
#define DEBUG
/*#define RELEASE*/

/*LINUX ����ϵͳ*/
#define OS_LINUX

/**/


/************ ��makefile �� -D ******/
/*ģ��ARM��ĳ���, ��-DARM_CPU*/
/*#define ARM_CPU*/

/*ģ��ZSP��ĳ���, ��-DZSP_CPU*/
/*#define ZSP_CPU*/


/*�Ƿ�ִ�д�ӡ����*/
#define LOGQUEUE

/*�Ƿ���Arm��ZSPͨѶ��ͳ��*/
#define AZ_STAT

/*�������logqueue*/
#define MULTI_TASK_LOGQUEUE

/*ARM��ZSP��ͨѶʹ���жϷ�ʽ*/
#define USE_AZ_INT

#define STAT


#if 0
/*������Ƶʱ��, �����ߴ�*/
#define AZ_POOL_LOW
#define AZ_MAGIC_LOW
#endif

//#define H264STREAM_CORRECT
/*RingBuffer ͳ��*/
#define RBSTAT

#define BITSTREAM_ENC_CHECKSUM
#define BITSTREAM_DEC_CHECKSUM


#define RTSP_VOD

#define SAVE_VOICE 1


/*֧����·�㲥*/
#define MPLAYER_NETWORK

#define DEMO_MEDIA

#define DEMO_VOICE
#define DEMO_VIDEO_ENC
#define DEMO_VIDEO_DEC

/*ͬ������ʹ��cond, ����ʹ��sem*/
//#define SYNC_USE_COND

/*ARMд��Ϣ�ص�ʱ���ж�ZSP����������Ϣ���Ƿ��д*/
#define AZPOOLS_X

/*HI3510V100 ���ɲ���*/
#define HI3510V100

#if defined(IMAGESIZE_CIF)
#define CONFIG_VIU_CAPTURE_DOWNSCALING //CIF
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif /* __HI_CONFIG_H__ */
