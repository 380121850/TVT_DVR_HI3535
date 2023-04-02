/******************************************************************************
*    Copyright (c) 2009-2012 by Hisi.
*    All rights reserved.
******************************************************************************/

#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/errno.h>

#include "../spi_ids.h"
#include "hisfc350.h"

#ifdef CONFIG_HI3535_SDK_2050
/* MXIC QE(bit) include in Status Register */
#define MX_SPI_NOR_SR_QE_SHIFT	6
#define MX_SPI_NOR_SR_QE_MASK	(1 << MX_SPI_NOR_SR_QE_SHIFT)

/*****************************************************************************/
/*
   enable QE bit if QUAD read write is supported by SPI
*/
static int spi_mx25l25635e_qe_enable(struct hisfc_spi *spi)
{
	unsigned char status, op;
	unsigned int reg;
	const char *str[] = {"Disable", "Enable"};
	struct hisfc_host *host = (struct hisfc_host *)spi->host;

	op = hisfc350_is_quad(spi);

	if (DEBUG_SPI_QE)
		printk(KERN_INFO "* Start SPI Nor %s Quad.\n", str[op]);

	status = spi_general_get_flash_register(spi, SPI_CMD_RDSR);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Read status %#x, val[%#x]\n", SPI_CMD_RDSR,
				status);
	if (((status & MX_SPI_NOR_SR_QE_MASK) >> MX_SPI_NOR_SR_QE_SHIFT)
			== op) {
		if (DEBUG_SPI_QE)
			printk(KERN_INFO "* Quad was %sd!\n", str[op]);
		return op;
	}

	spi->driver->write_enable(spi);

	if (op)
		status |= MX_SPI_NOR_SR_QE_MASK;
	else
		status &= ~MX_SPI_NOR_SR_QE_MASK;
	hisfc_write(host, HISFC350_CMD_DATABUF0, status);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Set DATA[%#x]%#x\n", HISFC350_CMD_DATABUF0,
				status);

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_WRSR);
	if (DEBUG_SPI_QE)
		printk(KERN_INFO "  Set INS[%#x]%#x\n", HISFC350_CMD_INS,
				SPI_CMD_WRSR);

	reg = HISFC350_CMD_CONFIG_DATA_CNT(SPI_NOR_SR_LEN)
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

		status = spi_general_get_flash_register(spi, SPI_CMD_RDSR);
		if (((status & MX_SPI_NOR_SR_QE_MASK) >> MX_SPI_NOR_SR_QE_SHIFT)
				== op)
			printk(KERN_INFO "* SPI %s Quad succeed.\n", str[op]);
		else
			DBG_MSG("%s Quad failed! [%#x]\n", str[op], status);
	}

	return op;
}

#else /* CONFIG_HI3535_SDK_2050 */

#define SPI_BRWR	0x17
#define SPI_EN4B	0x80
#define SPI_EX4B	0x00

#define MX_SPI_CMD_SR_QE   (0x40)

static int spi_mx25l25635e_qe_enable(struct hisfc_spi *spi)
{
	struct hisfc_host *host = (struct hisfc_host *)spi->host;
	unsigned int regval = 0;
	unsigned int qe_op = 0;
	if (hisfc350_is_quad(spi))
		qe_op = MX_SPI_CMD_SR_QE;
	else
		qe_op = SPI_CMD_SR_XQE;

	spi->driver->write_enable(spi);

	hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_WRSR);
	hisfc_write(host, HISFC350_CMD_DATABUF0, qe_op);

	hisfc_write(host, HISFC350_CMD_CONFIG,
			HISFC350_CMD_CONFIG_MEM_IF_TYPE(spi->
				write->iftype)
			| HISFC350_CMD_CONFIG_DATA_CNT(1)
			| HISFC350_CMD_CONFIG_DATA_EN
			| HISFC350_CMD_CONFIG_DUMMY_CNT(spi->
				write->dummy)
			| HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
			| HISFC350_CMD_CONFIG_START);

	HISFC350_CMD_WAIT_CPU_FINISH(host);

	spi->driver->wait_ready(spi);

	if (DEBUG_SPI) {
		hisfc_write(host, HISFC350_CMD_INS, SPI_CMD_RDSR);

		hisfc_write(host, HISFC350_CMD_CONFIG,
				HISFC350_CMD_CONFIG_SEL_CS(spi->chipselect)
				| HISFC350_CMD_CONFIG_DATA_CNT(1)
				| HISFC350_CMD_CONFIG_DATA_EN
				| HISFC350_CMD_CONFIG_RW_READ
				| HISFC350_CMD_CONFIG_START);
		HISFC350_CMD_WAIT_CPU_FINISH(host);
		regval = hisfc_read(host, HISFC350_CMD_DATABUF0);
		printk(KERN_INFO "QEbit = 0x40? : 0x%x\n", regval);
		if ((regval & MX_SPI_CMD_SR_QE))
			printk(KERN_INFO "QE bit enable success\n");
		else
			printk(KERN_INFO "QE bit enable failed\n");
	}
	return 0;
}
#endif /* CONFIG_HI3535_SDK_2050 */
