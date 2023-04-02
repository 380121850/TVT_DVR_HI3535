/*****************************************************************************
 *    Copyright (c) 2009-2011 by HiC
 *    All rights reserved.
 *****************************************************************************/

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include "../spi_ids.h"
#include "hisfc350.h"
#define DEBUG_SPI 0
#ifdef CONFIG_HI3535_SDK_2050
#define DEBUG_GET_SR 0
#define DEBUG_SPI_QE 0
#else
#define QE_CORRECT_VALUE  (0x02)
#endif /* CONFIG_HI3535_SDK_2050 */

#ifdef CONFIG_HI3535_SDK_2050
/*****************************************************************************/
u_char spi_general_get_flash_register(struct hisfc_spi *spi, u_char cmd)
{
	unsigned char status;
	unsigned int regval;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	if (DEBUG_GET_SR)
		printk(KERN_INFO "* Start get flash Register %#x.\n", cmd);

	hisfc_write(host, HISFC350_CMD_INS, cmd);
	if (DEBUG_GET_SR)
		printk(KERN_INFO "  Set INS[%#x]%#x\n", HISFC350_CMD_INS, cmd);

	regval = HISFC350_CMD_CONFIG_DATA_CNT(SPI_NOR_SR_LEN)
		| HISFC350_CMD_CONFIG_RW_READ
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START;

	hisfc_write(host, HISFC350_CMD_CONFIG, regval);
	if (DEBUG_GET_SR)
		printk(KERN_INFO "  Set CONFIG[%#x]%#x\n", HISFC350_CMD_CONFIG,
				regval);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	status = hisfc_read(host, HISFC350_CMD_DATABUF0);
	if (DEBUG_GET_SR)
		printk(KERN_INFO "* Get flash Register %#x, val[%#x]\n", cmd,
				status);

	return status;
}
#endif /* CONFIG_HI3535_SDK_2050 */
/*****************************************************************************/
static int spi_general_wait_ready(struct hisfc_spi *spi)
{
#ifdef CONFIG_HI3535_SDK_2050
	unsigned long status;
	unsigned long deadline = jiffies + HISFC350_MAX_READY_WAIT_JIFFIES;

	do {
		status = spi_general_get_flash_register(spi, SPI_CMD_RDSR);
		if (!(status & SPI_CMD_SR_WIP))
			return 0;

		cond_resched();

	} while (!time_after_eq(jiffies, deadline));
#else /* CONFIG_HI3535_SDK_2050 */
	unsigned long regval;
	unsigned long deadline = jiffies + HISFC350_MAX_READY_WAIT_JIFFIES;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	do {
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_RDSR);
		hisfc_write(host, HISFC350_CMD_CONFIG,
			HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
			| HISFC350_CMD_CONFIG_DATA_CNT(1)
			| HISFC350_CMD_CONFIG_DATA_EN
			| HISFC350_CMD_CONFIG_RW_READ
			| HISFC350_CMD_CONFIG_START);

		HISFC350_CMD_WAIT_CPU_FINISH(host);
		regval = hisfc_read(host, HISFC350_CMD_DATABUF0);
		if (!(regval & SPI_CMD_SR_WIP))
			return 0;
		cond_resched();

	} while (!time_after_eq(jiffies, deadline));
#endif

	printk(KERN_ERR "Wait spi flash ready timeout.\n");

	return 1;
}
/*****************************************************************************/
static int spi_general_write_enable(struct hisfc_spi *spi)
{
#ifdef CONFIG_HI3535_SDK_2050
#else
	unsigned int regval = 0;
#endif
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_WREN);

#ifdef CONFIG_HI3535_SDK_2050
	hisfc_write(host, HISFC350_CMD_CONFIG,
			HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
			| HISFC350_CMD_CONFIG_START);
#else
	regval = HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START;
	hisfc_write(host, HISFC350_CMD_CONFIG, regval);
#endif /* CONFIG_HI3535_SDK_2050 */

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	return 0;
}
/*****************************************************************************/
/*
  enable 4byte adress for SPI which memory more than 16M
*/
static int spi_general_entry_4addr(struct hisfc_spi *spi, int enable)
{
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	if (spi->addrcycle != SPI_4BYTE_ADDR_LEN)
		return 0;

	if (enable)
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_EN4B);
	else
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_EX4B);

	hisfc_write(host, HISFC350_CMD_CONFIG,
		HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	host->set_host_addr_mode(host, enable);

	return 0;
}
/*****************************************************************************/
/*
  configure prepared for dma or bus read or write mode
*/
static int spi_general_bus_prepare(struct hisfc_spi *spi, int op)
{
	unsigned int regval = 0;
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

#ifdef HISFCV350_SUPPORT_BUS_WRITE
	regval |= HISFC350_BUS_CONFIG1_WRITE_EN;
#endif
	regval |= HISFC350_BUS_CONFIG1_WRITE_INS(spi->write->cmd);
	regval |= HISFC350_BUS_CONFIG1_WRITE_DUMMY_CNT(spi->write->dummy);
	regval |= HISFC350_BUS_CONFIG1_WRITE_IF_TYPE(spi->write->iftype);

#ifdef HISFCV350_SUPPORT_BUS_READ
	regval |= HISFC350_BUS_CONFIG1_READ_EN;
#endif
	regval |= HISFC350_BUS_CONFIG1_READ_PREF_CNT(0);
	regval |= HISFC350_BUS_CONFIG1_READ_INS(spi->read->cmd);
	regval |= HISFC350_BUS_CONFIG1_READ_DUMMY_CNT(spi->read->dummy);
	regval |= HISFC350_BUS_CONFIG1_READ_IF_TYPE(spi->read->iftype);

	hisfc_write(host, HISFC350_BUS_CONFIG1, regval);
	hisfc_write(host, HISFC350_BUS_CONFIG2,
		HISFC350_BUS_CONFIG2_WIP_LOCATE(0));
	if (op == READ)
		host->set_system_clock(host, spi->read, TRUE);
	else if (op == WRITE)
		host->set_system_clock(host, spi->write, TRUE);

	return 0;
}
/*****************************************************************************/
/*
  judge whether SPI support QUAD read write or not
*/
static int hisfc350_is_quad(struct hisfc_spi *spi)
{
#ifdef CONFIG_HI3535_SDK_2050
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "SPI read if[%d] write if[%d]\n",
			spi->read->iftype, spi->write->iftype);
#endif /* CONFIG_HI3535_SDK_2050 */

	if (spi->write->iftype == 5 || spi->write->iftype == 6
		|| spi->write->iftype == 7 || spi->read->iftype == 5
		|| spi->read->iftype == 6 || spi->read->iftype == 7) {
#ifdef CONFIG_HI3535_SDK_2050
#else
		if (DEBUG_SPI)
			printk(KERN_INFO "4r4w SPI...............\n");
#endif /* CONFIG_HI3535_SDK_2050 */
		return 1;
	}
	if (DEBUG_SPI)
		printk(KERN_INFO "2r2w or 1r1w SPI...............\n");
	return 0;
}

/*****************************************************************************/
/*
   enable QE bit if QUAD read write is supported by SPI
*/
static int spi_general_qe_enable(struct hisfc_spi *spi)
{
#ifdef CONFIG_HI3535_SDK_2050
	unsigned char status, config, op;
	unsigned int reg;
	const char *str[] = {"Disable", "Enable"};
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	op = hisfc350_is_quad(spi);

	if (DEBUG_SPI_QE)
		printk(KERN_INFO "* Start SPI Nor %s Quad.\n", str[op]);

	config = spi_general_get_flash_register(spi, SPI_CMD_RDCR);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Read config %#x, val[%#x]\n", SPI_CMD_RDCR,
				config);
	if (((config & SPI_NOR_CR_QE_MASK) >> SPI_NOR_CR_QE_SHIFT) == op) {
		if (DEBUG_SPI_QE)
			printk(KERN_INFO "* Quad was %sd!\n", str[op]);
		return op;
	}

	status = spi_general_get_flash_register(spi, SPI_CMD_RDSR);
	reg = (config << SPI_NOR_CR_SHIFT) | status;
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Read CR/SR[%#x]\n", reg);

	spi->driver->write_enable(spi);

	if (op)
		reg |= (SPI_NOR_CR_QE_MASK << SPI_NOR_CR_SHIFT);
	else
		reg &= ~(SPI_NOR_CR_QE_MASK << SPI_NOR_CR_SHIFT);
	hisfc_write(host, HISFC350_CMD_DATABUF0, reg);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Set DATA[%#x]%#x\n", HISFC350_CMD_DATABUF0,
				reg);

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_WRSR);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Set INS[%#x]%#x\n", HISFC350_CMD_INS,
				SPI_CMD_WRSR);

	reg = HISFC350_CMD_CONFIG_DATA_CNT(SPI_NOR_SR_LEN + SPI_NOR_CR_LEN)
		| HISFC350_CMD_CONFIG_DATA_EN
		| HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
		| HISFC350_CMD_CONFIG_START;
	hisfc_write(host, HISFC350_CMD_CONFIG, reg);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Set CONFIG[%#x]%#x\n", HISFC350_CMD_CONFIG,
				reg);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	if (DEBUG_SPI_QE) {
		spi->driver->wait_ready(spi);

		config = spi_general_get_flash_register(spi, SPI_CMD_RDCR);
		if (((config & SPI_NOR_CR_QE_MASK) >> SPI_NOR_CR_QE_SHIFT)
				== op)
			printk(KERN_INFO "* SPI Quad %s succeed. [%#x]\n",
					str[op], config);
		else
			DBG_MSG("%s Quad failed! [%#x]\n", str[op], config);
	}

	return op;
#else /* CONFIG_HI3535_SDK_2050 */
	struct hisfc_host *host = (struct hisfc_host *)spi->host;
	unsigned int regval = 0;
	unsigned int qe_op = 0;
	if (hisfc350_is_quad(spi))
		qe_op = SPI_CMD_SR_QE;
	else
		qe_op = SPI_CMD_SR_XQE;

	spi->driver->write_enable(spi);

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_WRSR);
	hisfc_write(host, HISFC350_CMD_DATABUF0, qe_op);

	hisfc_write(host, HISFC350_CMD_CONFIG,
			HISFC350_CMD_CONFIG_MEM_IF_TYPE(spi->
				write->iftype)
			| HISFC350_CMD_CONFIG_DATA_CNT(2)
			| HISFC350_CMD_CONFIG_DATA_EN
			| HISFC350_CMD_CONFIG_DUMMY_CNT(spi->
				write->dummy)
			| HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
			| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	spi->driver->wait_ready(spi);

	if (DEBUG_SPI) {
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_RDSR2);

		hisfc_write(host, HISFC350_CMD_CONFIG,
				HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
				| HISFC350_CMD_CONFIG_DATA_CNT(2)
				| HISFC350_CMD_CONFIG_DATA_EN
				| HISFC350_CMD_CONFIG_RW_READ
				| HISFC350_CMD_CONFIG_START);
		HISFC350_CMD_WAIT_CPU_FINISH(host);
		regval = hisfc_read(host, HISFC350_CMD_DATABUF0);
		printk(KERN_INFO "QEbit = 202? : 0x%x\n", regval);
		if ((regval & QE_CORRECT_VALUE))
			printk(KERN_INFO "QE bit enable success\n");
		else
			printk(KERN_INFO "QE bit enable failed\n");
	}
	return 0;
#endif /* CONFIG_HI3535_SDK_2050 */
}
