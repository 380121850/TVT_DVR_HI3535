/******************************************************************************
*    Copyright (c) 2009-2010 by Hisi
*    All rights reserved.
* ***
*    written by Czy. 2010-10-13 14:01:24
*
******************************************************************************/

#include "oob_config.h"

const char *nand_controller_version = "Nand Controller V504";


/*****************************************************************************/
struct nand_oob_free oobfree_def[] =
{
    {2, 30}, {0, 0},
};
/*****************************************************************************/
static struct nand_oob_free oobfree_2k_1bit[] =
{
    {22, 42}, {0,0},
};
/*****************************************************************************/
static struct oob_info hinfc504_oob_info[] =
{
    {NANDIP_V301, pt_pagesize_2K, et_ecc_1bit,    64,  oobfree_2k_1bit},
    {NANDIP_V504, pt_pagesize_2K, et_ecc_4bit,    64,  oobfree_def},
    {NANDIP_V504, pt_pagesize_2K, et_ecc_8bytes,   88,  oobfree_def},
    {NANDIP_V504, pt_pagesize_2K, et_ecc_24bit1k, 128, oobfree_def},

    {NANDIP_V301, pt_pagesize_4K, et_ecc_1bit,    128, oobfree_def},
    {NANDIP_V301, pt_pagesize_4K, et_ecc_4bit,    128, oobfree_def},
    {NANDIP_V504, pt_pagesize_4K, et_ecc_8bytes,   144, oobfree_def},
    {NANDIP_V301, pt_pagesize_4K, et_ecc_24bit1k, 200, oobfree_def},

    {NANDIP_V301, pt_pagesize_8K, et_ecc_24bit1k, 368, oobfree_def},
    {NANDIP_V504, pt_pagesize_8K, et_ecc_40bit1k, 600, oobfree_def},

    {0},
};
/*****************************************************************************/

struct oob_info * get_oob_info(enum page_type pagetype, 
    enum ecc_type ecctype)
{
    struct oob_info *info = hinfc504_oob_info; 
    
    for (; info->freeoob; info++)
    {
        if (info->ecctype == ecctype
            && info->pagetype == pagetype)
        {
            return info;
        }
    }

    return 0;
}
/*****************************************************************************/

char *get_ecctype_str(enum ecc_type ecctype)
{
    char *ecctype_str[et_ecc_last+1] = { (char *)"None", (char *)"1bit", 
        (char *)"4bit", (char *)"8Bytes", (char *)"24bits/1K", (char *)"40bits/1K"};
    if (ecctype < et_ecc_none || ecctype > et_ecc_last)
    {
        ecctype = et_ecc_last;
    }
    return ecctype_str[ecctype];
}
/*****************************************************************************/

char *get_pagesize_str(enum page_type pagetype)
{
    char *pagesize_str[pt_pagesize_last+1] = {(char *)"512", (char *)"2K",
        (char *)"4K", (char *)"8K", (char *)"unknown" };
    if (pagetype < pt_pagesize_512 || pagetype > pt_pagesize_last)
    {
        pagetype = pt_pagesize_last;
    }
    return pagesize_str[pagetype];
}
/*****************************************************************************/

unsigned int get_pagesize(enum page_type pagetype)
{
    unsigned int pagesize[] = {512, 2048, 4096, 8192, 0};
    return pagesize[pagetype];
}
