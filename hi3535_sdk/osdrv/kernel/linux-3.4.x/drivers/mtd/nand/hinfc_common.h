/******************************************************************************
 *    NAND Flash Controller V504 Device Driver
 *    Copyright (c) 2009-2010 by Hisilicon.
 *    All rights reserved.
 * ***
 *    Create By Czyong.
 *
******************************************************************************/

#ifndef HINFC_COMMONH
#define HINFC_COMMONH
/******************************************************************************/
#ifdef CONFIG_HI3535_SDK_2050
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/string_helpers.h>
#include <asm/setup.h>
#include <linux/module.h>
#endif /* CONFIG_HI3535_SDK_2050 */
#define HINFC_VER_300                   (0x300)
#define HINFC_VER_301                   (0x301)
#define HINFC_VER_310                   (0x310)
#define HINFC_VER_504                   (0x504)
#define HINFC_VER_505                   (0x505)
#define HINFC_VER_600                   (0x600)
#ifdef CONFIG_HI3535_SDK_2050
#define HINFC_VER_610                   (0x610)
#define HINFC_VER_620                   (0x620)

#define HISNFC_VER_100                  (0x400)
#endif /* CONFIG_HI3535_SDK_2050 */

#define _512B                               (512)

#ifdef CONFIG_HI3535_SDK_2050
#ifndef SPI_IDSH
#define _1K                             (1024)
#define _2K                             (2048)
#define _4K                             (4096)
#define _8K                             (8192)
#define _16K                            (16384)
#define _32K                            (32768)
#define _64K                            (0x10000UL)
#define _128K                           (0x20000UL)
#define _256K                           (0x40000UL)
#define _512K                           (0x80000UL)
#define _1M                             (0x100000UL)
#define _2M                             (0x200000UL)
#define _4M                             (0x400000UL)
#define _8M                             (0x800000UL)
#define _16M                            (0x1000000UL)
#define _32M                            (0x2000000UL)
#define _64M                            (0x4000000UL)
#endif /* SPI_IDSH */

#define _128M                           (0x8000000UL)
#define _256M                           (0x10000000UL)
#define _512M                           (0x20000000UL)
#define _1G                             (0x40000000ULL)
#define _2G                             (0x80000000ULL)
#define _4G                             (0x100000000ULL)
#define _8G                             (0x200000000ULL)
#define _16G                            (0x400000000ULL)
#define _64G                            (0x1000000000ULL)

#define NAND_PAGE_512B                   0
#define NAND_PAGE_1K                     1
#define NAND_PAGE_2K                     2
#define NAND_PAGE_4K                     3
#define NAND_PAGE_8K                     4
#define NAND_PAGE_16K                    5
#define NAND_PAGE_32K                    6

#define NAND_ECC_0BIT                    0
#define NAND_ECC_1BIT_512                1
#define NAND_ECC_4BIT                    2
#define NAND_ECC_4BIT_512                3
#define NAND_ECC_4BYTE                   4
#define NAND_ECC_8BIT                    5
#define NAND_ECC_8BIT_512                6
#define NAND_ECC_8BYTE                   7
#define NAND_ECC_13BIT                   8
#define NAND_ECC_16BIT                   9
#define NAND_ECC_18BIT                   10
#define NAND_ECC_24BIT                   11
#define NAND_ECC_27BIT                   12
#define NAND_ECC_28BIT                   13
#define NAND_ECC_32BIT                   14
#define NAND_ECC_40BIT                   15
#define NAND_ECC_41BIT                   16
#define NAND_ECC_42BIT                   17
#define NAND_ECC_48BIT                   18
#define NAND_ECC_60BIT                   19
#define NAND_ECC_64BIT                   20
#define NAND_ECC_72BIT                   21
#define NAND_ECC_80BIT                   22
#else /* CONFIG_HI3535_SDK_2050 */
#define _2K                                 (2048)
#define _4K                                 (4096)
#define _8K                                 (8192)
#define _16K                                (16384)
#endif /* CONFIG_HI3535_SDK_2050 */

#define ERSTR_HARDWARE  "Hardware configuration error. "
#define ERSTR_DRIVER    "Driver does not support. "

#ifdef CONFIG_HI3535_SDK_2050
#define DISABLE                          0
#define ENABLE                           1

#define IS_NANDC_HW_AUTO(_host)         ((_host)->flags & NANDC_HW_AUTO)
#define IS_NANDC_CONFIG_DONE(_host)     ((_host)->flags & NANDC_CONFIG_DONE)
#define IS_NANDC_SYNC_BOOT(_host)       ((_host)->flags & NANDC_IS_SYNC_BOOT)

#define IS_NAND_RANDOM(_dev)         ((_dev)->flags & NAND_RANDOMIZER)
#define IS_NAND_ONLY_SYNC(_dev)      ((_dev)->flags & NAND_MODE_ONLY_SYNC)
#define IS_NAND_SYNC_ASYNC(_dev)     ((_dev)->flags & NAND_MODE_SYNC_ASYNC)
#define IS_NAND_ONFI(_dev)           ((_dev)->flags & NAND_IS_ONFI)
#endif /* CONFIG_HI3535_SDK_2050 */
/*****************************************************************************/

enum ecc_type {
	et_ecc_none    = 0x00,
	et_ecc_1bit    = 0x01,
	et_ecc_4bit    = 0x02,
	et_ecc_4bytes  = 0x02,
#ifdef CONFIG_HI3535_SDK_2050
	et_ecc_8bit    = 0x03,
#endif /* CONFIG_HI3535_SDK_2050 */
	et_ecc_8bytes  = 0x03,
	et_ecc_24bit1k = 0x04,
	et_ecc_40bit1k = 0x05,
#ifdef CONFIG_HI3535_SDK_2050
	et_ecc_64bit1k = 0x06,
#endif /* CONFIG_HI3535_SDK_2050 */
};

enum page_type {
	pt_pagesize_512   = 0x00,
	pt_pagesize_2K    = 0x01,
	pt_pagesize_4K    = 0x02,
	pt_pagesize_8K    = 0x03,
	pt_pagesize_16K   = 0x04,
};

struct page_page_ecc_info {
	enum page_type pagetype;
	enum ecc_type  ecctype;
	unsigned int oobsize;
	struct nand_ecclayout *layout;
};

#ifdef CONFIG_HI3535_SDK_2050
struct nand_config_info {
	unsigned pagetype;
	unsigned ecctype;
	unsigned oobsize;
	struct nand_ecclayout *layout;
};
#endif /* CONFIG_HI3535_SDK_2050 */

struct hinfc_host;
#ifdef CONFIG_HI3535_SDK_2050
struct nand_sync {

#define SET_NAND_SYNC_TYPE(_mfr, _onfi, _version) \
	((((_mfr) & 0xFF) << 16) | (((_version) & 0xFF) << 8) \
	 | ((_onfi) & 0xFF))

#define GET_NAND_SYNC_TYPE_MFR(_type) (((_type) >> 16) & 0xFF)
#define GET_NAND_SYNC_TYPE_VER(_type) (((_type) >> 8) & 0xFF)
#define GET_NAND_SYNC_TYPE_INF(_type) ((_type) & 0xFF)

#define NAND_TYPE_ONFI_23_MICRON    \
	SET_NAND_SYNC_TYPE(NAND_MFR_MICRON, NAND_IS_ONFI, 0x23)
#define NAND_TYPE_ONFI_30_MICRON    \
	SET_NAND_SYNC_TYPE(NAND_MFR_MICRON, NAND_IS_ONFI, 0x30)
#define NAND_TYPE_TOGGLE_TOSHIBA    \
	SET_NAND_SYNC_TYPE(NAND_MFR_TOSHIBA, 0, 0)
#define NAND_TYPE_TOGGLE_SAMSUNG    \
	SET_NAND_SYNC_TYPE(NAND_MFR_SAMSUNG, 0, 0)

#define NAND_TYPE_TOGGLE_10         SET_NAND_SYNC_TYPE(0, 0, 0x10)
#define NAND_TYPE_ONFI_30           SET_NAND_SYNC_TYPE(0, NAND_IS_ONFI, 0x30)
#define NAND_TYPE_ONFI_23           SET_NAND_SYNC_TYPE(0, NAND_IS_ONFI, 0x23)

	int type;
	int (*enable)(struct nand_chip *chip);
	int (*disable)(struct nand_chip *chip);
};

#endif /* CONFIG_HI3535_SDK_2050 */

struct read_retry_t {
	int type;
	int count;
	int (*set_rr_param)(struct hinfc_host *host, int param);
	int (*get_rr_param)(struct hinfc_host *host);
#ifdef CONFIG_HI3535_SDK_2050
	int (*reset_rr_param)(struct hinfc_host *host);
#endif /* CONFIG_HI3535_SDK_2050 */
};

struct nand_flash_dev_ex {
	struct nand_flash_dev flash_dev;

	char *start_type;
	unsigned char ids[8];
	int oobsize;
	int ecctype;
#ifdef CONFIG_HI3535_SDK_2050
	/* (Controller) support ecc/page detect, driver don't need detect */
#define NANDC_HW_AUTO                         0x01
	/* (Controller) support ecc/page detect,
	 * and current ecc/page config finish */
#define NANDC_CONFIG_DONE                     0x02
	/* (Controller) is sync, default is async */
#define NANDC_IS_SYNC_BOOT                    0x04

/* (NAND) need randomizer */
#define NAND_RANDOMIZER                       0x10
/* (NAND) is ONFI interface, combine with sync/async symble */
#define NAND_IS_ONFI                          0x20
/* (NAND) support async and sync, such micron onfi, toshiba toggle 1.0 */
#define NAND_MODE_SYNC_ASYNC                  0x40
/* (NAND) support only sync, such samsung sync. */
#define NAND_MODE_ONLY_SYNC                   0x80

#define NAND_CHIP_MICRON   (NAND_MODE_SYNC_ASYNC | NAND_IS_ONFI)
/* This NAND is async, or sync/async, default is async mode,
 * toggle1.0 interface */
#define NAND_CHIP_TOSHIBA_TOGGLE_10  (NAND_MODE_SYNC_ASYNC)
/* This NAND is only sync mode, toggle2.0 interface */
#define NAND_CHIP_TOSHIBA_TOGGLE_20   (NAND_MODE_ONLY_SYNC)
/* This NAND is only sync mode */
#define NAND_CHIP_SAMSUNG  (NAND_MODE_ONLY_SYNC)

	int flags;
#endif /* CONFIG_HI3535_SDK_2050 */
	int is_randomizer;

#define NAND_RR_NONE                     0x0
#define NAND_RR_HYNIX_BG_BDIE            0x10
#define NAND_RR_HYNIX_BG_CDIE            0x11
#define NAND_RR_HYNIX_CG_ADIE            0x12
#define NAND_RR_MICRON                   0x2
#define NAND_RR_SAMSUNG                  0x3
#define NAND_RR_TOSHIBA_24nm             0x4
	int read_retry_type;
};

/*****************************************************************************/

char *get_ecctype_str(enum ecc_type ecctype);

char *get_pagesize_str(enum page_type pagetype);

unsigned int get_pagesize(enum page_type pagetype);

#ifdef CONFIG_HI3535_SDK_2050
const char *nand_ecc_name(int type);

const char *nand_page_name(int type);

int nandpage_size2type(int size);

int nandpage_type2size(int size);

extern struct nand_flash_dev *(*nand_get_flash_type_func)(struct mtd_info *mtd,
	struct nand_chip *chip, struct nand_flash_dev_ex *flash_dev_ex);

extern int (*nand_oob_resize)(struct mtd_info *mtd, struct nand_chip *chip,
	struct nand_flash_dev_ex *nand_dev);

struct nand_flash_dev *nand_get_flash_type_ex(struct mtd_info *mtd,
	struct nand_chip *chip, struct nand_flash_dev_ex *nand_dev);

void nand_show_info(struct nand_flash_dev_ex *nand_dev,
	struct mtd_info *mtd, char *vendor, char *chipname);

#else
extern int (*nand_oob_resize)(struct mtd_info *mtd, struct nand_chip *chip,
	struct nand_flash_dev_ex *flash_dev_ex);
struct nand_flash_dev *nand_get_flash_type_ex(struct mtd_info *mtd,
	struct nand_chip *chip, struct nand_flash_dev_ex *flash_dev_ex);

void nand_show_info(struct nand_flash_dev_ex *flash_dev_ex,
	struct mtd_info *mtd, char *vendor, char *chipname);
#endif /* CONFIG_HI3535_SDK_2050 */

void nand_show_chip(struct nand_chip *chip);

#ifdef CONFIG_HI3535_SDK_2050
extern char *nand_dbgfs_options;
#endif /* CONFIG_HI3535_SDK_2050 */
/******************************************************************************/
#endif /* HINFC_COMMONH */
