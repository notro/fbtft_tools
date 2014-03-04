/*
 * Adds a stmpe device
 *
 * Copyright (C) 2014, Noralf Tronnes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/stmpe.h>
#include <linux/spi/spi.h>

#define DRVNAME "stmpe_device"

static unsigned int verbose = 0;
module_param(verbose, uint, 0);
MODULE_PARM_DESC(verbose, "0-2");

/* SPI arguments */
static unsigned busnum = 0;
module_param(busnum, uint, 0);
MODULE_PARM_DESC(busnum, "SPI bus number (default: 0)");

static unsigned cs = 0;
module_param(cs, uint, 0);
MODULE_PARM_DESC(cs, "SPI chip select (default: 0)");

static unsigned speed = 500 * 1000;
module_param(speed, uint, 0);
MODULE_PARM_DESC(speed, "SPI speed (default: 500kHz)");

static int mode = SPI_MODE_0;
module_param(mode, int, 0);
MODULE_PARM_DESC(mode, "SPI mode (default: SPI_MODE_0)");

/* Base arguments */
static char *chip;
module_param(chip, charp, 0);
MODULE_PARM_DESC(chip, "Chip - modalias field");

static char *blocks;
module_param(blocks, charp, 0);
MODULE_PARM_DESC(blocks, "List of blocks to use (supported: gpio,ts)");

static int irq_base;
module_param(irq_base, int, 0);
MODULE_PARM_DESC(irq_base, "base IRQ number");

static unsigned int irq_trigger = IRQF_TRIGGER_FALLING;
module_param(irq_trigger, int, 0);
MODULE_PARM_DESC(irq_trigger, "IRQ trigger to use for the interrupt to the host (default: IRQF_TRIGGER_FALLING)");

static int irq_gpio = -1;
module_param(irq_gpio, int, 0);
MODULE_PARM_DESC(irq_gpio, "gpio number over which irq will be requested (default: disabled)");

static int autosleep_timeout;
module_param(autosleep_timeout, int, 0);
MODULE_PARM_DESC(autosleep_timeout, "inactivity timeout in milliseconds for autosleep (default: disabled)");

static bool irq_pullup = false;
#ifdef CONFIG_ARCH_BCM2708
module_param(irq_pullup, bool, 0);
MODULE_PARM_DESC(irq_pulldown, "Enable internal pull up resistor for irq");
#endif

/* GPIO block */
static int gpio_base = -1;
module_param(gpio_base, int, 0);
MODULE_PARM_DESC(gpio_base, "first gpio number assigned (default: auto)");

static unsigned norequest_mask;
module_param(norequest_mask, uint, 0);
MODULE_PARM_DESC(norequest_mask, "bitmask specifying which GPIOs should _not_ be requestable");

/* Touch block */
static unsigned sample_time = 4;
module_param(sample_time, uint, 0);
MODULE_PARM_DESC(sample_time, "ADC converstion time in number of clocks (default: 4 -> 80 clocks)");

static unsigned mod_12b;
module_param(mod_12b, uint, 0);
MODULE_PARM_DESC(mod_12b, "ADC Bit mode (default: 0 -> 10 bit");

static unsigned ref_sel;
module_param(ref_sel, uint, 0);
MODULE_PARM_DESC(ref_sel, "ADC reference source (default: 0 -> internal reference)");

static unsigned adc_freq;
module_param(adc_freq, uint, 0);
MODULE_PARM_DESC(adc_freq, "ADC Clock speed (default: 0 -> 1.625 MHz)");

static unsigned ave_ctrl;
module_param(ave_ctrl, uint, 0);
MODULE_PARM_DESC(ave_ctrl, "Sample average control (default: 0 -> 1 sample)");

static unsigned touch_det_delay = 3;
module_param(touch_det_delay, uint, 0);
MODULE_PARM_DESC(touch_det_delay, "Touch detect interrupt delay (default: 3 -> 500 us)");

static unsigned settling = 2;
module_param(settling, uint, 0);
MODULE_PARM_DESC(settling, "Panel driver settling time (default: 2 -> 500 us)");

static unsigned fraction_z = 7;
module_param(fraction_z, uint, 0);
MODULE_PARM_DESC(fraction_z, "Length of the fractional part in z (default: 7)");

static unsigned i_drive;
module_param(i_drive, uint, 0);
MODULE_PARM_DESC(i_drive, "current limit value of the touchscreen drivers (default: 0 -> 20 mA typical 35 mA max)");


#define pr_pdata(sym)  pr_info(DRVNAME":   "#sym" = %d\n", pdata->sym)

static struct stmpe_platform_data pdata_stmpe_device = {
	.gpio = &(struct stmpe_gpio_platform_data) { 0, },
	.ts = &(struct stmpe_ts_platform_data) { 0, },
};

static struct spi_board_info stmpe_device = {
	.platform_data = &pdata_stmpe_device,
};

struct spi_device *stmpe_spi_device = NULL;

#ifdef CONFIG_ARCH_BCM2708
static void gpio_pull(unsigned pin, unsigned pud)
{
#define	GPIO_PUD     37
#define	GPIO_PUDCLK0 38
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
	u32 *gpio = ioremap(0x20200000, SZ_16K);

	if (verbose > 1)
		pr_info(DRVNAME": %s(%d, %d)\n", __func__, pin, pud);

	/* set as input */
	INP_GPIO(pin);
	/* set pull tri/down/up */
	*(gpio + GPIO_PUD) = pud & 3 ;		
	udelay(5);
	*(gpio + GPIO_PUDCLK0 + (pin>31 ? 1 : 0) ) = 1 << (pin & 31);
	udelay(5);
	*(gpio + GPIO_PUD) = 0;
	udelay(5);
	*(gpio + GPIO_PUDCLK0 + (pin>31 ? 1 : 0) ) = 0;
	udelay(5);

	iounmap(gpio);
#undef INP_GPIO
}
#else
static void gpio_pull(unsigned pin, unsigned pud)
{
}
#endif

static int spi_device_found(struct device *dev, void *data)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	pr_info(DRVNAME":    %s %s %dkHz %d bits mode=0x%02X\n", spi->modalias, dev_name(dev), spi->max_speed_hz/1000, spi->bits_per_word, spi->mode);

	return 0;
}

static void pr_spi_devices(void)
{
	pr_info(DRVNAME": SPI devices registered:\n");
	bus_for_each_dev(&spi_bus_type, NULL, NULL, spi_device_found);
	pr_info(DRVNAME":\n");
}

#ifdef MODULE
static void stmpe_device_spi_delete(struct spi_master *master, unsigned cs)
{
	struct device *dev;
	char str[32];

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);

	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (dev) {
		if (verbose)
			pr_info(DRVNAME": Deleting %s\n", str);
		device_del(dev);
	}
}

static int stmpe_device_spi_device_register(struct spi_board_info *spi)
{
	struct spi_master *master;

	master = spi_busnum_to_master(spi->bus_num);
	if (!master) {
		pr_err(DRVNAME ":  spi_busnum_to_master(%d) returned NULL\n",
								spi->bus_num);
		return -EINVAL;
	}
	/* make sure it's available */
	stmpe_device_spi_delete(master, spi->chip_select);
	stmpe_spi_device = spi_new_device(master, spi);
	put_device(&master->dev);
	if (!stmpe_spi_device) {
		pr_err(DRVNAME ":    spi_new_device() returned NULL\n");
		return -EPERM;
	}
	return 0;
}
#else
static int stmpe_device_spi_device_register(struct spi_board_info *spi)
{
	return spi_register_board_info(spi, 1);
}
#endif

static int __init stmpe_device_init(void)
{
	struct stmpe_platform_data *pdata = &pdata_stmpe_device;
	char *tmp;
	int ret;

	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);

	if (chip == NULL) {
#ifdef MODULE
		pr_err(DRVNAME": missing module parameter: 'chip'\n");
		return -EINVAL;
#else
		return 0;
#endif
	}

	while ((tmp = strsep(&blocks, ","))) {
		if (strcmp(tmp, "gpio") == 0) {
			pdata->blocks |= STMPE_BLOCK_GPIO;
		} else if (strcmp(tmp, "ts") == 0) {
			pdata->blocks |= STMPE_BLOCK_TOUCHSCREEN;
		} else {
			pr_err(DRVNAME": unrecognized value in module parameter 'blocks': %s\n", tmp);
			return -EINVAL;
		}
	}

	if (verbose > 1)
		pr_spi_devices(); /* print list of registered SPI devices */

	/* set SPI values */
	strlcpy(stmpe_device.modalias, chip, 32);
	stmpe_device.max_speed_hz = speed;
	stmpe_device.bus_num = busnum;
	stmpe_device.chip_select = cs;
	stmpe_device.mode = mode;

	/* set platform_data values */
	if (irq_gpio >= 0) {
		pdata->irq_over_gpio = true;
		pdata->irq_base = irq_base;
		pdata->irq_trigger = irq_trigger;
		pdata->irq_gpio = irq_gpio;
	}
	pdata->autosleep_timeout = autosleep_timeout;

	pdata->gpio->gpio_base = gpio_base;
	pdata->gpio->norequest_mask = norequest_mask;

	pdata->ts->sample_time = sample_time;
	pdata->ts->mod_12b = mod_12b;
	pdata->ts->ref_sel = ref_sel;
	pdata->ts->adc_freq = adc_freq;
	pdata->ts->ave_ctrl = ave_ctrl;
	pdata->ts->touch_det_delay = touch_det_delay;
	pdata->ts->settling = settling;
	pdata->ts->fraction_z = fraction_z;
	pdata->ts->i_drive = i_drive;

	if (verbose) {
		pr_info(DRVNAME": Settings:\n");
		pr_info(DRVNAME":   chip = %s\n", stmpe_device.modalias);
		if (pdata->irq_over_gpio) {
			pr_pdata(irq_base);
			pr_pdata(irq_trigger);
			pr_pdata(irq_gpio);
		}
		if (pdata->blocks && STMPE_BLOCK_GPIO) {
			pr_pdata(gpio->gpio_base);
			pr_pdata(gpio->norequest_mask);
		}
		if (pdata->blocks && STMPE_BLOCK_TOUCHSCREEN) {
			pr_pdata(ts->sample_time);
			pr_pdata(ts->mod_12b);
			pr_pdata(ts->ref_sel);
			pr_pdata(ts->adc_freq);
			pr_pdata(ts->ave_ctrl);
			pr_pdata(ts->touch_det_delay);
			pr_pdata(ts->settling);
			pr_pdata(ts->fraction_z);
			pr_pdata(ts->i_drive);
		}
	}

	if (pdata->irq_over_gpio && irq_pullup) {
		gpio_pull(pdata->irq_gpio, 2);
	}

	ret = stmpe_device_spi_device_register(&stmpe_device);
	if (ret) {
		pr_err(DRVNAME": failed to register SPI device\n");
		return ret;
	}

	if (verbose)
		pr_spi_devices();

	return 0;
}

static void __exit stmpe_device_exit(void)
{
	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);

	if (stmpe_spi_device) {
		device_del(&stmpe_spi_device->dev);
		kfree(stmpe_spi_device);
	}
}

module_init(stmpe_device_init);
module_exit(stmpe_device_exit);

MODULE_DESCRIPTION("Adds a stmpe device");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
