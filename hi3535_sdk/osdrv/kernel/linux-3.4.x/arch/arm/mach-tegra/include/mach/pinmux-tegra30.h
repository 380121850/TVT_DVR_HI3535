/*
 * linux/arch/arm/mach-tegra/include/mach/pinmux-tegra30.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010,2011 Nvidia, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_TEGRA_PINMUX_TEGRA30_H
#define __MACH_TEGRA_PINMUX_TEGRA30_H

enum tegra_pingroup {
	TEGRA_PINGROUP_ULPI_DATA0 = 0,
	TEGRA_PINGROUP_ULPI_DATA1,
	TEGRA_PINGROUP_ULPI_DATA2,
	TEGRA_PINGROUP_ULPI_DATA3,
	TEGRA_PINGROUP_ULPI_DATA4,
	TEGRA_PINGROUP_ULPI_DATA5,
	TEGRA_PINGROUP_ULPI_DATA6,
	TEGRA_PINGROUP_ULPI_DATA7,
	TEGRA_PINGROUP_ULPI_CLK,
	TEGRA_PINGROUP_ULPI_DIR,
	TEGRA_PINGROUP_ULPI_NXT,
	TEGRA_PINGROUP_ULPI_STP,
	TEGRA_PINGROUP_DAP3_FS,
	TEGRA_PINGROUP_DAP3_DIN,
	TEGRA_PINGROUP_DAP3_DOUT,
	TEGRA_PINGROUP_DAP3_SCLK,
	TEGRA_PINGROUP_GPIO_PV0,
	TEGRA_PINGROUP_GPIO_PV1,
	TEGRA_PINGROUP_SDMMC1_CLK,
	TEGRA_PINGROUP_SDMMC1_CMD,
	TEGRA_PINGROUP_SDMMC1_DAT3,
	TEGRA_PINGROUP_SDMMC1_DAT2,
	TEGRA_PINGROUP_SDMMC1_DAT1,
	TEGRA_PINGROUP_SDMMC1_DAT0,
	TEGRA_PINGROUP_GPIO_PV2,
	TEGRA_PINGROUP_GPIO_PV3,
	TEGRA_PINGROUP_CLK2_OUT,
	TEGRA_PINGROUP_CLK2_REQ,
	TEGRA_PINGROUP_LCD_PWR1,
	TEGRA_PINGROUP_LCD_PWR2,
	TEGRA_PINGROUP_LCD_SDIN,
	TEGRA_PINGROUP_LCD_SDOUT,
	TEGRA_PINGROUP_LCD_WR_N,
	TEGRA_PINGROUP_LCD_CS0_N,
	TEGRA_PINGROUP_LCD_DC0,
	TEGRA_PINGROUP_LCD_SCK,
	TEGRA_PINGROUP_LCD_PWR0,
	TEGRA_PINGROUP_LCD_PCLK,
	TEGRA_PINGROUP_LCD_DE,
	TEGRA_PINGROUP_LCD_HSYNC,
	TEGRA_PINGROUP_LCD_VSYNC,
	TEGRA_PINGROUP_LCD_D0,
	TEGRA_PINGROUP_LCD_D1,
	TEGRA_PINGROUP_LCD_D2,
	TEGRA_PINGROUP_LCD_D3,
	TEGRA_PINGROUP_LCD_D4,
	TEGRA_PINGROUP_LCD_D5,
	TEGRA_PINGROUP_LCD_D6,
	TEGRA_PINGROUP_LCD_D7,
	TEGRA_PINGROUP_LCD_D8,
	TEGRA_PINGROUP_LCD_D9,
	TEGRA_PINGROUP_LCD_D10,
	TEGRA_PINGROUP_LCD_D11,
	TEGRA_PINGROUP_LCD_D12,
	TEGRA_PINGROUP_LCD_D13,
	TEGRA_PINGROUP_LCD_D14,
	TEGRA_PINGROUP_LCD_D15,
	TEGRA_PINGROUP_LCD_D16,
	TEGRA_PINGROUP_LCD_D17,
	TEGRA_PINGROUP_LCD_D18,
	TEGRA_PINGROUP_LCD_D19,
	TEGRA_PINGROUP_LCD_D20,
	TEGRA_PINGROUP_LCD_D21,
	TEGRA_PINGROUP_LCD_D22,
	TEGRA_PINGROUP_LCD_D23,
	TEGRA_PINGROUP_LCD_CS1_N,
	TEGRA_PINGROUP_LCD_M1,
	TEGRA_PINGROUP_LCD_DC1,
	TEGRA_PINGROUP_HDMI_INT,
	TEGRA_PINGROUP_DDC_SCL,
	TEGRA_PINGROUP_DDC_SDA,
	TEGRA_PINGROUP_CRT_HSYNC,
	TEGRA_PINGROUP_CRT_VSYNC,
	TEGRA_PINGROUP_VI_D0,
	TEGRA_PINGROUP_VI_D1,
	TEGRA_PINGROUP_VI_D2,
	TEGRA_PINGROUP_VI_D3,
	TEGRA_PINGROUP_VI_D4,
	TEGRA_PINGROUP_VI_D5,
	TEGRA_PINGROUP_VI_D6,
	TEGRA_PINGROUP_VI_D7,
	TEGRA_PINGROUP_VI_D8,
	TEGRA_PINGROUP_VI_D9,
	TEGRA_PINGROUP_VI_D10,
	TEGRA_PINGROUP_VI_D11,
	TEGRA_PINGROUP_VI_PCLK,
	TEGRA_PINGROUP_VI_MCLK,
	TEGRA_PINGROUP_VI_VSYNC,
	TEGRA_PINGROUP_VI_HSYNC,
	TEGRA_PINGROUP_UART2_RXD,
	TEGRA_PINGROUP_UART2_TXD,
	TEGRA_PINGROUP_UART2_RTS_N,
	TEGRA_PINGROUP_UART2_CTS_N,
	TEGRA_PINGROUP_UART3_TXD,
	TEGRA_PINGROUP_UART3_RXD,
	TEGRA_PINGROUP_UART3_CTS_N,
	TEGRA_PINGROUP_UART3_RTS_N,
	TEGRA_PINGROUP_GPIO_PU0,
	TEGRA_PINGROUP_GPIO_PU1,
	TEGRA_PINGROUP_GPIO_PU2,
	TEGRA_PINGROUP_GPIO_PU3,
	TEGRA_PINGROUP_GPIO_PU4,
	TEGRA_PINGROUP_GPIO_PU5,
	TEGRA_PINGROUP_GPIO_PU6,
	TEGRA_PINGROUP_GEN1_I2C_SDA,
	TEGRA_PINGROUP_GEN1_I2C_SCL,
	TEGRA_PINGROUP_DAP4_FS,
	TEGRA_PINGROUP_DAP4_DIN,
	TEGRA_PINGROUP_DAP4_DOUT,
	TEGRA_PINGROUP_DAP4_SCLK,
	TEGRA_PINGROUP_CLK3_OUT,
	TEGRA_PINGROUP_CLK3_REQ,
	TEGRA_PINGROUP_GMI_WP_N,
	TEGRA_PINGROUP_GMI_IORDY,
	TEGRA_PINGROUP_GMI_WAIT,
	TEGRA_PINGROUP_GMI_ADV_N,
	TEGRA_PINGROUP_GMI_CLK,
	TEGRA_PINGROUP_GMI_CS0_N,
	TEGRA_PINGROUP_GMI_CS1_N,
	TEGRA_PINGROUP_GMI_CS2_N,
	TEGRA_PINGROUP_GMI_CS3_N,
	TEGRA_PINGROUP_GMI_CS4_N,
	TEGRA_PINGROUP_GMI_CS6_N,
	TEGRA_PINGROUP_GMI_CS7_N,
	TEGRA_PINGROUP_GMI_AD0,
	TEGRA_PINGROUP_GMI_AD1,
	TEGRA_PINGROUP_GMI_AD2,
	TEGRA_PINGROUP_GMI_AD3,
	TEGRA_PINGROUP_GMI_AD4,
	TEGRA_PINGROUP_GMI_AD5,
	TEGRA_PINGROUP_GMI_AD6,
	TEGRA_PINGROUP_GMI_AD7,
	TEGRA_PINGROUP_GMI_AD8,
	TEGRA_PINGROUP_GMI_AD9,
	TEGRA_PINGROUP_GMI_AD10,
	TEGRA_PINGROUP_GMI_AD11,
	TEGRA_PINGROUP_GMI_AD12,
	TEGRA_PINGROUP_GMI_AD13,
	TEGRA_PINGROUP_GMI_AD14,
	TEGRA_PINGROUP_GMI_AD15,
	TEGRA_PINGROUP_GMI_A16,
	TEGRA_PINGROUP_GMI_A17,
	TEGRA_PINGROUP_GMI_A18,
	TEGRA_PINGROUP_GMI_A19,
	TEGRA_PINGROUP_GMI_WR_N,
	TEGRA_PINGROUP_GMI_OE_N,
	TEGRA_PINGROUP_GMI_DQS,
	TEGRA_PINGROUP_GMI_RST_N,
	TEGRA_PINGROUP_GEN2_I2C_SCL,
	TEGRA_PINGROUP_GEN2_I2C_SDA,
	TEGRA_PINGROUP_SDMMC4_CLK,
	TEGRA_PINGROUP_SDMMC4_CMD,
	TEGRA_PINGROUP_SDMMC4_DAT0,
	TEGRA_PINGROUP_SDMMC4_DAT1,
	TEGRA_PINGROUP_SDMMC4_DAT2,
	TEGRA_PINGROUP_SDMMC4_DAT3,
	TEGRA_PINGROUP_SDMMC4_DAT4,
	TEGRA_PINGROUP_SDMMC4_DAT5,
	TEGRA_PINGROUP_SDMMC4_DAT6,
	TEGRA_PINGROUP_SDMMC4_DAT7,
	TEGRA_PINGROUP_SDMMC4_RST_N,
	TEGRA_PINGROUP_CAM_MCLK,
	TEGRA_PINGROUP_GPIO_PCC1,
	TEGRA_PINGROUP_GPIO_PBB0,
	TEGRA_PINGROUP_CAM_I2C_SCL,
	TEGRA_PINGROUP_CAM_I2C_SDA,
	TEGRA_PINGROUP_GPIO_PBB3,
	TEGRA_PINGROUP_GPIO_PBB4,
	TEGRA_PINGROUP_GPIO_PBB5,
	TEGRA_PINGROUP_GPIO_PBB6,
	TEGRA_PINGROUP_GPIO_PBB7,
	TEGRA_PINGROUP_GPIO_PCC2,
	TEGRA_PINGROUP_JTAG_RTCK,
	TEGRA_PINGROUP_PWR_I2C_SCL,
	TEGRA_PINGROUP_PWR_I2C_SDA,
	TEGRA_PINGROUP_KB_ROW0,
	TEGRA_PINGROUP_KB_ROW1,
	TEGRA_PINGROUP_KB_ROW2,
	TEGRA_PINGROUP_KB_ROW3,
	TEGRA_PINGROUP_KB_ROW4,
	TEGRA_PINGROUP_KB_ROW5,
	TEGRA_PINGROUP_KB_ROW6,
	TEGRA_PINGROUP_KB_ROW7,
	TEGRA_PINGROUP_KB_ROW8,
	TEGRA_PINGROUP_KB_ROW9,
	TEGRA_PINGROUP_KB_ROW10,
	TEGRA_PINGROUP_KB_ROW11,
	TEGRA_PINGROUP_KB_ROW12,
	TEGRA_PINGROUP_KB_ROW13,
	TEGRA_PINGROUP_KB_ROW14,
	TEGRA_PINGROUP_KB_ROW15,
	TEGRA_PINGROUP_KB_COL0,
	TEGRA_PINGROUP_KB_COL1,
	TEGRA_PINGROUP_KB_COL2,
	TEGRA_PINGROUP_KB_COL3,
	TEGRA_PINGROUP_KB_COL4,
	TEGRA_PINGROUP_KB_COL5,
	TEGRA_PINGROUP_KB_COL6,
	TEGRA_PINGROUP_KB_COL7,
	TEGRA_PINGROUP_CLK_32K_OUT,
	TEGRA_PINGROUP_SYS_CLK_REQ,
	TEGRA_PINGROUP_CORE_PWR_REQ,
	TEGRA_PINGROUP_CPU_PWR_REQ,
	TEGRA_PINGROUP_PWR_INT_N,
	TEGRA_PINGROUP_CLK_32K_IN,
	TEGRA_PINGROUP_OWR,
	TEGRA_PINGROUP_DAP1_FS,
	TEGRA_PINGROUP_DAP1_DIN,
	TEGRA_PINGROUP_DAP1_DOUT,
	TEGRA_PINGROUP_DAP1_SCLK,
	TEGRA_PINGROUP_CLK1_REQ,
	TEGRA_PINGROUP_CLK1_OUT,
	TEGRA_PINGROUP_SPDIF_IN,
	TEGRA_PINGROUP_SPDIF_OUT,
	TEGRA_PINGROUP_DAP2_FS,
	TEGRA_PINGROUP_DAP2_DIN,
	TEGRA_PINGROUP_DAP2_DOUT,
	TEGRA_PINGROUP_DAP2_SCLK,
	TEGRA_PINGROUP_SPI2_MOSI,
	TEGRA_PINGROUP_SPI2_MISO,
	TEGRA_PINGROUP_SPI2_CS0_N,
	TEGRA_PINGROUP_SPI2_SCK,
	TEGRA_PINGROUP_SPI1_MOSI,
	TEGRA_PINGROUP_SPI1_SCK,
	TEGRA_PINGROUP_SPI1_CS0_N,
	TEGRA_PINGROUP_SPI1_MISO,
	TEGRA_PINGROUP_SPI2_CS1_N,
	TEGRA_PINGROUP_SPI2_CS2_N,
	TEGRA_PINGROUP_SDMMC3_CLK,
	TEGRA_PINGROUP_SDMMC3_CMD,
	TEGRA_PINGROUP_SDMMC3_DAT0,
	TEGRA_PINGROUP_SDMMC3_DAT1,
	TEGRA_PINGROUP_SDMMC3_DAT2,
	TEGRA_PINGROUP_SDMMC3_DAT3,
	TEGRA_PINGROUP_SDMMC3_DAT4,
	TEGRA_PINGROUP_SDMMC3_DAT5,
	TEGRA_PINGROUP_SDMMC3_DAT6,
	TEGRA_PINGROUP_SDMMC3_DAT7,
	TEGRA_PINGROUP_PEX_L0_PRSNT_N,
	TEGRA_PINGROUP_PEX_L0_RST_N,
	TEGRA_PINGROUP_PEX_L0_CLKREQ_N,
	TEGRA_PINGROUP_PEX_WAKE_N,
	TEGRA_PINGROUP_PEX_L1_PRSNT_N,
	TEGRA_PINGROUP_PEX_L1_RST_N,
	TEGRA_PINGROUP_PEX_L1_CLKREQ_N,
	TEGRA_PINGROUP_PEX_L2_PRSNT_N,
	TEGRA_PINGROUP_PEX_L2_RST_N,
	TEGRA_PINGROUP_PEX_L2_CLKREQ_N,
	TEGRA_PINGROUP_HDMI_CEC,
	TEGRA_MAX_PINGROUP,
};

enum tegra_drive_pingroup {
	TEGRA_DRIVE_PINGROUP_AO1 = 0,
	TEGRA_DRIVE_PINGROUP_AO2,
	TEGRA_DRIVE_PINGROUP_AT1,
	TEGRA_DRIVE_PINGROUP_AT2,
	TEGRA_DRIVE_PINGROUP_AT3,
	TEGRA_DRIVE_PINGROUP_AT4,
	TEGRA_DRIVE_PINGROUP_AT5,
	TEGRA_DRIVE_PINGROUP_CDEV1,
	TEGRA_DRIVE_PINGROUP_CDEV2,
	TEGRA_DRIVE_PINGROUP_CSUS,
	TEGRA_DRIVE_PINGROUP_DAP1,
	TEGRA_DRIVE_PINGROUP_DAP2,
	TEGRA_DRIVE_PINGROUP_DAP3,
	TEGRA_DRIVE_PINGROUP_DAP4,
	TEGRA_DRIVE_PINGROUP_DBG,
	TEGRA_DRIVE_PINGROUP_LCD1,
	TEGRA_DRIVE_PINGROUP_LCD2,
	TEGRA_DRIVE_PINGROUP_SDIO2,
	TEGRA_DRIVE_PINGROUP_SDIO3,
	TEGRA_DRIVE_PINGROUP_SPI,
	TEGRA_DRIVE_PINGROUP_UAA,
	TEGRA_DRIVE_PINGROUP_UAB,
	TEGRA_DRIVE_PINGROUP_UART2,
	TEGRA_DRIVE_PINGROUP_UART3,
	TEGRA_DRIVE_PINGROUP_VI1,
	TEGRA_DRIVE_PINGROUP_SDIO1,
	TEGRA_DRIVE_PINGROUP_CRT,
	TEGRA_DRIVE_PINGROUP_DDC,
	TEGRA_DRIVE_PINGROUP_GMA,
	TEGRA_DRIVE_PINGROUP_GMB,
	TEGRA_DRIVE_PINGROUP_GMC,
	TEGRA_DRIVE_PINGROUP_GMD,
	TEGRA_DRIVE_PINGROUP_GME,
	TEGRA_DRIVE_PINGROUP_GMF,
	TEGRA_DRIVE_PINGROUP_GMG,
	TEGRA_DRIVE_PINGROUP_GMH,
	TEGRA_DRIVE_PINGROUP_OWR,
	TEGRA_DRIVE_PINGROUP_UAD,
	TEGRA_DRIVE_PINGROUP_GPV,
	TEGRA_DRIVE_PINGROUP_DEV3,
	TEGRA_DRIVE_PINGROUP_CEC,
	TEGRA_MAX_DRIVE_PINGROUP,
};

#endif
