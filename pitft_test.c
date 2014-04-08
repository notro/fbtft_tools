/*
 * PiTFT test utility
 *
 * This tool uses the BCM2835 library to access SPI and GPIO on the Raspberry Pi.
 * The library accesses the hardware registers directly, and thus bypasses
 * the Linux SPI driver and gpiolib.
 * http://www.airspayce.com/mikem/bcm2835/index.html
 *
 * Copyright (C) 2014, Noralf Tronnes
 *
 * BSD License
 *
 */

#include <bcm2835.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define u8 uint8_t

int verbose = 0;

/*
 * LCD controller
 */

#define DC_PIN 25

#define ILI9340_SLPOUT 0x11
#define ILI9340_GAMMASET 0x26
#define ILI9340_DISPOFF 0x28
#define ILI9340_DISPON 0x29
#define ILI9340_MADCTL 0x36
#define ILI9340_MADCTL_MX 0x40
#define ILI9340_MADCTL_BGR 0x08
#define ILI9340_PIXFMT 0x3A
#define ILI9340_FRMCTR1 0xB1
#define ILI9340_FRMCTR2 0xB2
#define ILI9340_FRMCTR3 0xB3
#define ILI9340_DFUNCTR 0xB6
#define ILI9340_PWCTR1 0xC0
#define ILI9340_PWCTR2 0xC1
#define ILI9340_VMCTR1 0xC5
#define ILI9340_VMCTR2 0xC7
#define ILI9340_GMCTRP1 0xE0
#define ILI9340_GMCTRN1 0xE1

#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))

#define write_reg(...)                                              \
do {                                                                     \
	write_register(NUMARGS(__VA_ARGS__), __VA_ARGS__); \
} while (0)

#define mdelay bcm2835_delay

void write_register(int len, ...)
{
	va_list args;
	int i;
	u8 buf[128];

	if (verbose) {
		printf("%s: ", __func__);
		va_start(args, len);
		for (i = 0; i < len; i++) {
			printf("%02X ", (u8)va_arg(args, unsigned int));
		}
		va_end(args);
		printf("\n");
	}

	va_start(args, len);
	buf[0] = (u8)va_arg(args, unsigned int);
	bcm2835_gpio_write(DC_PIN, LOW);
	mdelay(10);
	bcm2835_spi_transfern(buf, 1);

	len--;
	if (len) {
		bcm2835_gpio_write(DC_PIN, HIGH);
		i = len;
		for (i = 0; i < len; i++) {
			buf[i] = (u8)va_arg(args, unsigned int);
		}
		bcm2835_spi_transfern(buf, len);
	}
	va_end(args);
}

int init_display()
{
        write_reg(0x01); /* software reset */
        mdelay(5);
        write_reg(0x28); /* display off */

	/* startup sequence taken from Adafruit, registers are undocumented */
	write_reg(0xEF, 0x03, 0x80, 0x02);
	write_reg(0xCF, 0x00, 0xC1, 0x30);
	write_reg(0xED, 0x64, 0x03, 0x12, 0x81);
	write_reg(0xE8, 0x85, 0x00, 0x78);
	write_reg(0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02);
	write_reg(0xF7, 0x20);
	write_reg(0xEA, 0x00, 0x00);

	/* power control */
	write_reg(ILI9340_PWCTR1, 0x23); //VRH[5:0]
	write_reg(ILI9340_PWCTR2, 0x10); //SAP[2:0];BT[3:0]

	/* VCM control */
	write_reg(ILI9340_VMCTR1, 0x3e, 0x28);
	write_reg(ILI9340_VMCTR2, 0x86);
	write_reg(ILI9340_MADCTL, ILI9340_MADCTL_MX | ILI9340_MADCTL_BGR);
	write_reg(ILI9340_PIXFMT, 0x55);
        /* ------------frame rate----------------------------------- */
	write_reg(ILI9340_FRMCTR1, 0x00, 0x18);
 
	/* display function control */
	write_reg(ILI9340_DFUNCTR, 0x08, 0x82, 0x27);
 
	/* Gamma function disable */
	write_reg(0xF2, 0x00);
 
	/* gamma curve selected */
	write_reg(ILI9340_GAMMASET, 0x01);
 
	/* set gamma */
	write_reg(ILI9340_GMCTRP1, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
			  0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E,
			  0x09, 0x00);
  
	/* set gamma */
	write_reg(ILI9340_GMCTRN1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
			0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F);
        /* ------------Gamma---------------------------------------- */
        /* write_reg(0xF2, 0x08); */ /* Gamma Function Disable */
        //write_reg(0x26, 0x01);

	/* exit sleep */
	write_reg(ILI9340_SLPOUT);

	mdelay(100);

	/* display on */
	write_reg(ILI9340_DISPON);

	mdelay(20);

	return 0;
}

void set_addr_win(int xs, int ys, int xe, int ye)
{
	write_reg(0x2A, (xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF, xe & 0xFF);
	write_reg(0x2B, (ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF, ye & 0xFF);
	write_reg(0x2C);
}

void fill_display(unsigned int color)
{
	int i;
	u8 buf[128];

	set_addr_win(0, 0, 239, 319);
	bcm2835_gpio_write(DC_PIN, HIGH);
	for (i = 0; i < 320*240; i++) {
		buf[0] = (color >> 8);
		buf[1] = color & 0xFF;
		bcm2835_spi_transfern(buf, 2);
	}
}

void display_test()
{
	printf("\nTest writing to display controller\n");

	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16); /* 15.625MHz */
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);

	bcm2835_gpio_fsel(DC_PIN, BCM2835_GPIO_FSEL_OUTP);

	printf("  Initialize controller\n");
	init_display();
	printf("  Fill display with red color\n");
	fill_display(0b1111100000000000); /* RGB565 red */

	bcm2835_gpio_write(DC_PIN, LOW);
	bcm2835_gpio_fsel(DC_PIN, BCM2835_GPIO_FSEL_INPT);
}

/*
 * Touch controller
 */

#define IRQ_PIN 24

#define READ_CMD (1 << 7)


//#define STMPE811_IRQ_TOUCH_DET          0
//#define STMPE811_IRQ_FIFO_TH            1
//#define STMPE811_IRQ_FIFO_OFLOW         2
//#define STMPE811_IRQ_FIFO_FULL          3
//#define STMPE811_IRQ_FIFO_EMPTY         4
//#define STMPE811_IRQ_TEMP_SENS          5
//#define STMPE811_IRQ_ADC                6
//#define STMPE811_IRQ_GPIOC              7
//#define STMPE811_NR_INTERNAL_IRQS       8
//
#define STMPE811_REG_CHIP_ID            0x00
#define STMPE811_REG_SYS_CTRL1          0x03
#define STMPE811_REG_SYS_CTRL2          0x04
#define STMPE811_REG_SPI_CFG            0x08
//#define STMPE811_REG_INT_CTRL           0x09
//#define STMPE811_REG_INT_EN             0x0A
//#define STMPE811_REG_INT_STA            0x0B
//#define STMPE811_REG_GPIO_INT_EN        0x0C
//#define STMPE811_REG_GPIO_INT_STA       0x0D
#define STMPE811_REG_GPIO_SET_PIN       0x10
#define STMPE811_REG_GPIO_CLR_PIN       0x11
//#define STMPE811_REG_GPIO_MP_STA        0x12
#define STMPE811_REG_GPIO_DIR           0x13
//#define STMPE811_REG_GPIO_ED            0x14
//#define STMPE811_REG_GPIO_RE            0x15
//#define STMPE811_REG_GPIO_FE            0x16
#define STMPE811_REG_GPIO_AF            0x17

#define GPIO_2 (1 << 2)

unsigned int stmpe_chip_id()
{
	u8 buf[1];
	unsigned int id;

	buf[0] = READ_CMD | STMPE811_REG_CHIP_ID;
	bcm2835_spi_transfern(buf, 1);
	buf[0] = READ_CMD | (STMPE811_REG_CHIP_ID + 1);
	bcm2835_spi_transfern(buf, 1);
	id = buf[0] << 8;
	buf[0] = 0x00;
	bcm2835_spi_transfern(buf, 1);
	id |= buf[0];

	return id;
}

u8 stmpe_read_reg(u8 reg)
{
	u8 buf[1];

	if (verbose)
		printf("%s(reg=0x%02X) -> ", __func__, reg);
	buf[0] = READ_CMD | reg;
	bcm2835_spi_transfern(buf, 1);
	buf[0] = 0x00;
	bcm2835_spi_transfern(buf, 1);
	if (verbose)
		printf("0x%02X\n", buf[0]);

	return buf[0];
}

void stmpe_write_reg(u8 reg, u8 val)
{
	u8 buf[1];

	if (verbose)
		printf("%s(reg=0x%02X, val=0x%02X)\n", __func__, reg, val);
	buf[0] = reg;
	buf[1] = val;
	bcm2835_spi_transfern(buf, 2);
}


void touch_test()
{
	unsigned int id;
	int i;

	printf("\nTest communication with touch controller STMPE610\n");

	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512); /* 488 kHz */
	bcm2835_spi_chipSelect(BCM2835_SPI_CS1);

	id = stmpe_chip_id();
	if (id != 0x0811) {
		/* I don't understand why mode 0 doesn't work */
		if (verbose)
			printf("trying SPI mode 3\n");
		bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
		id = stmpe_chip_id();
	}

	id = stmpe_chip_id();
	printf("  Chip id: 0x%04x\n", id);

	if (id != 0x0811) {
		printf("\n\nTEST FAILED\nUnexpected chip id, should be 0x0811\n\n");
		return;
	}

	printf("  Verify that IRQ line is HIGH\n");
	bcm2835_gpio_fsel(IRQ_PIN, BCM2835_GPIO_FSEL_INPT);
	if (!bcm2835_gpio_lev(IRQ_PIN)) {
		printf("\n\nTEST FAILED\nIRQ pin should be HIGH\n\n");
		return;
	}

	/* reset controller */
	stmpe_write_reg(STMPE811_REG_SYS_CTRL1, 0b00000010);

	/* Turn on GPIO and TSC clocks */
	stmpe_write_reg(STMPE811_REG_SYS_CTRL2, 0b00000001);

//	for (i=0; i<0x10; i++) {
//		stmpe_read_reg(i);
//	}

	printf("  Blink backlight 3 times\n");
	/* set gpio mode for pin gpio-2 */
	stmpe_write_reg(STMPE811_REG_GPIO_AF, GPIO_2);
	/* set gpio direction to output */
	stmpe_write_reg(STMPE811_REG_GPIO_DIR, GPIO_2);
	for (i = 0; i < 3; i++) {
		stmpe_write_reg(STMPE811_REG_GPIO_CLR_PIN, GPIO_2);
		mdelay(300);
		stmpe_write_reg(STMPE811_REG_GPIO_SET_PIN, GPIO_2);
		mdelay(300);
	}
}


int main(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcmp(argv[1], "-v"))
			verbose = 1;
		else {
			printf("unknown argument, only -v for verbose supported\n");
			return 1;
		}
	}
	printf("PiTFT test utility by Noralf Tronnes\n");
	if (!bcm2835_init())
		return 1;

	bcm2835_spi_begin();

	display_test();
	touch_test();

	bcm2835_close();

	printf("\nNote: A reboot is needed after using this tool to restore SPI operation\n");
	return 0;
}
