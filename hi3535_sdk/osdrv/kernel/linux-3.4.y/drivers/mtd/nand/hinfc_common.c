/******************************************************************************
*    Copyright (c) 2009-2011 by Hisi.
*    All rights reserved.
* ***
*    Create by Czyong. 2011-12-03
*
******************************************************************************/

#include "hinfc_common_os.h"
#include "hinfc_common.h"

/*****************************************************************************/

int (*nand_oob_resize)(struct mtd_info *mtd, struct nand_chip *chip,
	struct nand_flash_dev_ex *flash_dev_ex) = NULL;

/*****************************************************************************/

char *get_ecctype_str(enum ecc_type ecctype)
{
	static char *ecctype_string[] = {
		"None", "1bit/512Byte", "4bits/512Byte", "8bits/512Byte",
		"24bits/1K", "40bits/1K", "unknown", "unknown"};
	return ecctype_string[(ecctype & 0x07)];
}
/*****************************************************************************/

char *get_pagesize_str(enum page_type pagetype)
{
	static char *pagesize_str[] = {
		"512", "2K", "4K", "8K", "16K", "unknown",
		"unknown", "unknown"};
	return pagesize_str[(pagetype & 0x07)];
}
/*****************************************************************************/

unsigned int get_pagesize(enum page_type pagetype)
{
	unsigned int pagesize[] = {
		_512B, _2K, _4K, _8K, _16K, 0, 0, 0};
	return pagesize[(pagetype & 0x07)];
}
