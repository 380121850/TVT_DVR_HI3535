#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "hi_comm_vo.h"
#include "hi_comm_vi.h"
#include "hi_comm_vpss.h"
#include "hi_type.h"

#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_vi.h"
#include "mpi_vo.h"
#include "mpi_vpss.h"

#define USAGE_HELP(void)\
{\
    printf("\n\tusage : %s para value group [chn] \n", argv[0]);    \
    printf("\n\texample 1 : set dci strength 60 to group 1 -- %s con 60 1 \n", argv[0]);    \
    printf("\n\texample 2 : set sharpen strength 50 to group 1 chn 2 -- %s chnsp 50 1 2\n", argv[0]);    \
    printf("\n\tpara: \n");    \
    printf("\t\tenIE   [0, disable; 1,enable]\n");   \
    printf("\t\tenNR   [0, disable; 1,enable]\n");    \
    printf("\t\tenDEI  [0, auto; 1, nodie; 2, die]\n");   \
    printf("\t\tenDCI  [0, disable; 1,enable]\n");    \
    printf("\t\tenHIST [0, disable; 1,enable]\n");    \
    printf("\t\tie     [IE强度，value:0~255, default:32]\n");   \
    printf("\t\tcon    [对比度，value:0~64, default:32]\n");   \
    printf("\t\tdei    [de-interlace强度，value:0~7, default:0]\n");   \
    printf("\t\tsf     [空域去噪强度，value:0~2047, default:3]\n");   \
    printf("\t\ttf     [时域去噪强度，value:0~63, default:1]\n");   \
    printf("\t\tdmb    [运动判断阈值，value:0~7, default:1]\n");   \
    printf("\t\tcf     [色度去噪强度，value:0~255, default:8]\n");   \
    printf("\t\tenSP   [0, disable; 1,enable]\n");   \
    printf("\t\tchnsp  [sp strength of chn，value:0~100, default:40]\n");   \
}

#define CHECK_RET(express,name)\
    do{\
        if (HI_SUCCESS != express)\
        {\
            printf("%s failed at %s: LINE: %d ! errno:0x%x \n", \
                name, __FUNCTION__, __LINE__, express);\
            return HI_FAILURE;\
        }\
    }while(0)


HI_S32 main(int argc, char *argv[])
{
    HI_S32 s32Ret;
    VPSS_GRP_ATTR_S stVpssGrpAttr = {0};
    VPSS_CHN_ATTR_S stVpssChnAttr = {0};
    VPSS_GRP_PARAM_S stVpssGrpParam = {0};
    VPSS_CHN_PARAM_S stVpssChnParam = {0};
    
    char paraTemp[16];
    HI_U32 value = 0;
    VPSS_GRP VpssGrp = 0XFFFFF;
    VPSS_CHN VpssChn = 0XFFFFF;
    const char* para = paraTemp;
    
    if (argc < 4)
    {
        USAGE_HELP();
        return -1;
    }
    
    strcpy(paraTemp, argv[1]);  
    value = atoi(argv[2]);
    VpssGrp = atoi(argv[3]);
    if (5 == argc)
    {
        VpssChn = atoi(argv[4]);
    }

    s32Ret = HI_MPI_VPSS_GetGrpAttr(VpssGrp, &stVpssGrpAttr);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_GetGrpAttr");

    s32Ret = HI_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_GetChnAttr");

    s32Ret = HI_MPI_VPSS_GetGrpParam(VpssGrp, &stVpssGrpParam);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_GetGrpParam");

    s32Ret = HI_MPI_VPSS_GetChnParam(VpssGrp, VpssChn, &stVpssChnParam);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_GetChnSpParam");

    if (0 == strcmp(para, "enIE"))
    {
       stVpssGrpAttr.bIeEn = value;
    }
    else if (0 == strcmp(para, "enNR"))
    {
        stVpssGrpAttr.bNrEn = value;
    }
    else if (0 == strcmp(para, "enDEI"))
    {
		if (value > 2)
		{
			printf("invalid enDEI param %d\n", value);
			return -1;
		}

        stVpssGrpAttr.enDieMode = (VPSS_DIE_MODE_E)value;
	}
	else if (0 == strcmp(para,"enHIST"))
	{
		stVpssGrpAttr.bHistEn = value;
	}
	else if (0 == strcmp(para,"enDCI"))
	{
		stVpssGrpAttr.bDciEn = value;
	}    
    else if (0 == strcmp(para, "enSP"))
    {
        stVpssChnAttr.bSpEn = value;
    }    
    else if (0 == strcmp(para, "ie"))
    {
        stVpssGrpParam.u32IeStrength = value;
    }

    else if (0 == strcmp(para, "con"))
    {
        stVpssGrpParam.u32Contrast = value;
    }
 
    else if (0 == strcmp(para, "dei"))
    {
        stVpssGrpParam.u32DieStrength = value;
    }

	else if (0 == strcmp(para, "sf"))
    {
        stVpssGrpParam.u32SfStrength = value;
    }    
    else if (0 == strcmp(para, "tf"))
    {
        stVpssGrpParam.u32TfStrength = value;
    }
    else if (0 == strcmp(para, "dmb"))
    {
        stVpssGrpParam.u32DeMotionBlurring = value;
    }
    else if (0 == strcmp(para, "cf"))
    {
	    stVpssGrpParam.u32CfStrength = value;
    }	
    else if (0 == strcmp(para, "chnsp"))    
    {
        stVpssChnParam.u32SpStrength = value;
    }
    else
    {
        printf("\n\t\033[0;31m!!!!!!!!!!!!!err para!!!!!!!!!!!!!\033[0;39m\n");
        USAGE_HELP();
    }

    s32Ret = HI_MPI_VPSS_SetGrpAttr(VpssGrp, &stVpssGrpAttr);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_SetGrpAttr");

    s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_SetChnAttr");

    s32Ret = HI_MPI_VPSS_SetGrpParam(VpssGrp, &stVpssGrpParam);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_SetGrpParam");

    s32Ret = HI_MPI_VPSS_SetChnParam(VpssGrp, VpssChn, &stVpssChnParam);
    CHECK_RET(s32Ret, "HI_MPI_VPSS_SetChpParam");

    printf("\t\tenIE   %d\n", stVpssGrpAttr.bIeEn);
    printf("\t\tenNR   %d\n", stVpssGrpAttr.bNrEn);
    printf("\t\tenDEI  %d\n", stVpssGrpAttr.enDieMode);
    printf("\t\tenHIST %d\n", stVpssGrpAttr.bHistEn);
    printf("\t\tenDCI  %d\n", stVpssGrpAttr.bDciEn);
    printf("\t\tie     %d\n", stVpssGrpParam.u32IeStrength);
    printf("\t\tcon    %d\n", stVpssGrpParam.u32Contrast);
    printf("\t\tdei    %d\n", stVpssGrpParam.u32DieStrength);


   	printf("\t\tsf     %d\n", stVpssGrpParam.u32SfStrength);
    printf("\t\ttf     %d\n", stVpssGrpParam.u32TfStrength);
    printf("\t\tdmb    %d\n", stVpssGrpParam.u32DeMotionBlurring);
    printf("\t\tcf     %d\n", stVpssGrpParam.u32CfStrength);

    printf("\t\tenSP   %d\n", stVpssChnAttr.bSpEn);
    printf("\t\tchnsp  %d\n", stVpssChnParam.u32SpStrength);

    return 0;
}

