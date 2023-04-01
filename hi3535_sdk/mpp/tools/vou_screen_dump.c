#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "mpi_sys.h"
#include "hi_comm_vb.h"
#include "mpi_vb.h"
#include "hi_comm_vo.h"
#include "mpi_vo.h"

#define MAX_FRM_CNT 256

static HI_S32 s_s32MemDev = 0;

HI_S32 memopen( void )
{
    if (s_s32MemDev <= 0)
    {
        s_s32MemDev = open ("/dev/mem", O_CREAT|O_RDWR|O_SYNC);
        if (s_s32MemDev <= 0)
        {
            return -1;
        }
    }
    return 0;
}

HI_VOID memclose()
{
	close(s_s32MemDev);
}

void * memmap( HI_U32 u32PhyAddr, HI_U32 u32Size )
{
    HI_U32 u32Diff;
    HI_U32 u32PagePhy;
    HI_U32 u32PageSize;
    HI_U8 * pPageAddr;

    u32PagePhy = u32PhyAddr & 0xfffff000;
    u32Diff = u32PhyAddr - u32PagePhy;

    /* size in page_size */
    u32PageSize = ((u32Size + u32Diff - 1) & 0xfffff000) + 0x1000;
    pPageAddr = mmap ((void *)0, u32PageSize, PROT_READ|PROT_WRITE,
                                    MAP_SHARED, s_s32MemDev, u32PagePhy);
    if (MAP_FAILED == pPageAddr )
    {
            return NULL;
    }
    return (void *) (pPageAddr + u32Diff);
}

HI_S32 memunmap(HI_VOID* pVirAddr, HI_U32 u32Size )
{
    HI_U32 u32PageAddr;
    HI_U32 u32PageSize;
    HI_U32 u32Diff;

    u32PageAddr = (((HI_U32)pVirAddr) & 0xfffff000);
    /* size in page_size */
    u32Diff     = (HI_U32)pVirAddr - u32PageAddr;
    u32PageSize = ((u32Size + u32Diff - 1) & 0xfffff000) + 0x1000;

    return munmap((HI_VOID*)u32PageAddr, u32PageSize);
}



/* sp420 ת��Ϊ p420 ; sp422 ת��Ϊ p422  */
void sample_yuv_dump(VIDEO_FRAME_S * pVBuf, FILE *pfd)
{
    unsigned int w, h;
    char * pVBufVirt_Y;
    char * pVBufVirt_C;
    char * pMemContent;
    unsigned char TmpBuff[4096];                //������ֵ̫С��ͼ��ܴ�Ļ��治��
    HI_U32 phy_addr,Ysize,Csize;
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight;/* ��Ϊplanar ��ʽʱ��UV�����ĸ߶� */

    Ysize = (pVBuf->u32Stride[0])*(pVBuf->u32Height);
    if (PIXEL_FORMAT_YUV_SEMIPLANAR_420 == enPixelFormat)
    {
        Csize = (pVBuf->u32Stride[1])*(pVBuf->u32Height)/2;    
        u32UvHeight = pVBuf->u32Height/2;
    }
    else
    {
        Csize = (pVBuf->u32Stride[1])*(pVBuf->u32Height);   
        u32UvHeight = pVBuf->u32Height;
    }

    phy_addr = pVBuf->u32PhyAddr[0];

    //printf("phy_addr:%x, size:%d\n", phy_addr, size);
    pVBufVirt_Y = (HI_CHAR *) HI_MPI_SYS_Mmap(phy_addr, Ysize);	
    if (NULL == pVBufVirt_Y)
    {
        return;
    }

    pVBufVirt_C = (HI_CHAR *) HI_MPI_SYS_Mmap(pVBuf->u32PhyAddr[1], Csize);
    if (NULL == pVBufVirt_C)
    {
        HI_MPI_SYS_Munmap(pVBufVirt_Y, Ysize); 
        return;
    }

    /* save Y ----------------------------------------------------------------*/
    fprintf(stderr, "saving......Y......");
    fflush(stderr);
    for(h=0; h<pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h*pVBuf->u32Stride[0];
        fwrite(pMemContent, pVBuf->u32Width, 1, pfd);
    }
    fflush(pfd);
    

    /* save U ----------------------------------------------------------------*/
    fprintf(stderr, "U......");
    fflush(stderr);
    for(h=0; h<u32UvHeight; h++)
    {
        pMemContent = pVBufVirt_C + h*pVBuf->u32Stride[1];

        pMemContent += 1;

        for(w=0; w<pVBuf->u32Width/2; w++)
        {
            TmpBuff[w] = *pMemContent;
            pMemContent += 2;
        }
        fwrite(TmpBuff, pVBuf->u32Width/2, 1, pfd);
    }
    fflush(pfd);

    /* save V ----------------------------------------------------------------*/
    fprintf(stderr, "V......");
    fflush(stderr);
    for(h=0; h<u32UvHeight; h++)    
    {
        pMemContent = pVBufVirt_C + h*pVBuf->u32Stride[1];

        for(w=0; w<pVBuf->u32Width/2; w++)
        {
            TmpBuff[w] = *pMemContent;
            pMemContent += 2;
        }
        fwrite(TmpBuff, pVBuf->u32Width/2, 1, pfd);
    }
    fflush(pfd);
    
    fprintf(stderr, "done %d!\n", pVBuf->u32TimeRef);
    fflush(stderr);
    
    HI_MPI_SYS_Munmap(pVBufVirt_Y, Ysize);  
    HI_MPI_SYS_Munmap(pVBufVirt_C, Csize);

}

HI_S32 SAMPLE_MISC_VoDump(VO_LAYER VoLayer, HI_U32 u32Cnt)
{
    HI_S32 i, s32Ret;
    VIDEO_FRAME_INFO_S stFrame;
    //VIDEO_FRAME_INFO_S astFrame[256];
    HI_CHAR szYuvName[128];
    HI_CHAR szPixFrm[10];
    FILE *pfd;

    printf("\nNOTICE: This tool only can be used for TESTING !!!\n");
    printf("usage: ./vo_screen_dump [VoLayer] [frmcnt]. sample: ./vo_screen_dump 0 1\n\n");

    /* Get Frame to make file name*/
    s32Ret = HI_MPI_VO_GetScreenFrame(VoLayer, &stFrame);
    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_VO(%d)_GetScreenFrame errno %#x\n", VoLayer, s32Ret);
        return -1;
    }

    /* make file name */
    strcpy(szPixFrm,
    (PIXEL_FORMAT_YUV_SEMIPLANAR_420 == stFrame.stVFrame.enPixelFormat)?"p420":"p422");
    sprintf(szYuvName, "./vo(%d)_%d_%d_%s_%d.yuv",VoLayer,
        stFrame.stVFrame.u32Width, stFrame.stVFrame.u32Height,szPixFrm,u32Cnt);
    printf("Dump YUV frame of vo(%d) to file: \"%s\"\n",VoLayer, szYuvName);

    HI_MPI_VO_ReleaseScreenFrame(VoLayer, &stFrame);

    /* open file */
    pfd = fopen(szYuvName, "wb");

    if (NULL == pfd)
    {
        return -1;
    }
    
    memopen();

    /* get VO frame  */
    for (i=0; i<u32Cnt; i++)
    {
        s32Ret = HI_MPI_VO_GetScreenFrame(VoLayer, &stFrame);
        if (HI_SUCCESS != s32Ret)
        {
            printf("get vo(%d) frame err\n", VoLayer);
            printf("only get %d frame\n", i);
            break;
        }
        printf("addr 0x%x\n", stFrame.stVFrame.u32PhyAddr[0]);
        /* save VO frame to file */
		sample_yuv_dump(&stFrame.stVFrame, pfd);

        /* release frame after using */
        s32Ret = HI_MPI_VO_ReleaseScreenFrame(VoLayer, &stFrame);
        if (HI_SUCCESS != s32Ret)
        {
            printf("Release vo(%d) frame err\n", VoLayer);
            printf("only get %d frame\n", i);
            break;
        }
    }

    memclose();

    fclose(pfd);

	return 0;
}

HI_S32 main(int argc, char *argv[])
{
    VO_LAYER VoLayer = 0;
    HI_U32 u32FrmCnt = 1;

	/* VO��Ƶ��ID*/
    if (argc > 1)
    {
        VoLayer = atoi(argv[1]);
    }
	/* ��Ҫ�ɼ���֡��Ŀ*/
    if (argc > 2)
    {
        u32FrmCnt = atoi(argv[2]);
    }

    SAMPLE_MISC_VoDump(VoLayer, u32FrmCnt);

	return HI_SUCCESS;
}

