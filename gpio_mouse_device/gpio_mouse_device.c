/*
 * Adds a gpio_mouse device
 *
 * Copyright (C) 2013, Noralf Tronnes
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
#include <linux/platform_device.h>
#include <linux/gpio_mouse.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DRVNAME "gpio_mouse_device"


static int scan_ms = 10;
module_param(scan_ms, int, 0);
MODULE_PARM_DESC(scan_ms, "integer in ms specifying the scan periode (default=10).");

static int polarity = 0;
module_param(polarity, int, 0);
MODULE_PARM_DESC(polarity, "Pin polarity, active high or low (default=0 high).");

static int up = -1;
module_param(up, int, 0);
MODULE_PARM_DESC(up, "GPIO line for up value.");

static int down = -1;
module_param(down, int, 0);
MODULE_PARM_DESC(down, "GPIO line for down value.");

static int left = -1;
module_param(left, int, 0);
MODULE_PARM_DESC(left, "GPIO line for left value.");

static int right = -1;
module_param(right, int, 0);
MODULE_PARM_DESC(right, "GPIO line for right value.");

static int bleft = -1;
module_param(bleft, int, 0);
MODULE_PARM_DESC(bleft, "GPIO line for left button.");

static int bmiddle = -1;
module_param(bmiddle, int, 0);
MODULE_PARM_DESC(bmiddle, "GPIO line for middle button.");

static int bright = -1;
module_param(bright, int, 0);
MODULE_PARM_DESC(bright, "GPIO line for right button.");

static bool pullup = false;
module_param(pullup, bool, 0);
MODULE_PARM_DESC(pulldown, "Enable internal pull up resistor for all (only on Raspberry Pi)");

static bool pulldown = false;
module_param(pulldown, bool, 0);
MODULE_PARM_DESC(pulldown, "Enable internal pull down resistor for all (only on Raspberry Pi)");

static unsigned int verbose = 0;
module_param(verbose, uint, 0);
MODULE_PARM_DESC(verbose, "0-1");


static void pdev_release(struct device *dev);

static struct gpio_mouse_platform_data pdata;

static struct platform_device gpio_mouse_device = {
  .name = "gpio_mouse",
  .id   = 0,
  .dev  = {
	.release = pdev_release,
    .platform_data = &pdata,
    },
};

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

static void gpio_pull_all(unsigned pud)
{
	if (verbose > 1)
		pr_info(DRVNAME": %s(%d)\n", __func__, pud);

	if (up > -1)
		gpio_pull(up, pud);
	if (down > -1)
		gpio_pull(down, pud);
	if (left > -1)
		gpio_pull(left, pud);
	if (right > -1)
		gpio_pull(right, pud);
	if (bleft > -1)
		gpio_pull(bleft, pud);
	if (bmiddle > -1)
		gpio_pull(bmiddle, pud);
	if (bright > -1)
		gpio_pull(bright, pud);
}
#else
static void gpio_pull_all(unsigned pud)
{
	pr_warning(DRVNAME": Pull up/down not supported on this platform\n");
}
#endif

static void pdev_release(struct device *dev)
{
	if (verbose > 1)
		pr_info(DRVNAME": %s()\n", __func__);

	if (pullup || pulldown)
		gpio_pull_all(0);
}

static int __init gpio_mouse_device_init(void)
{
	int ret;

	if (verbose)
		pr_info("\n\n"DRVNAME": %s()\n", __func__);

	if (pullup && pulldown) {
		pr_err(DRVNAME":  can't have both pullup and pulldown\n");
		return -EINVAL;
	}

	if (verbose && (pullup || pulldown))
		pr_info(DRVNAME":   Internal pull resistor: %s\n", pullup ? "up" : "down");

	/* set platform_data values */
	pdata.scan_ms = scan_ms;
	pdata.polarity = polarity;
	pdata.up = up;
	pdata.down = down;
	pdata.left = left;
	pdata.right = right;
	pdata.bleft = bleft;
	pdata.bmiddle = bmiddle;
	pdata.bright = bright;

	if (verbose) {
		pr_info(DRVNAME":   scan_ms:  %d\n", scan_ms);
		pr_info(DRVNAME":   polarity: %d\n", polarity);
		pr_info(DRVNAME":   up:       %d\n", up);
		pr_info(DRVNAME":   down:     %d\n", down);
		pr_info(DRVNAME":   left:     %d\n", left);
		pr_info(DRVNAME":   right:    %d\n", right);
		pr_info(DRVNAME":   bleft:    %d\n", bleft);
		pr_info(DRVNAME":   bmiddle:  %d\n", bmiddle);
		pr_info(DRVNAME":   bright:   %d\n", bright);
	}

	if (pullup || pulldown)
		gpio_pull_all(pulldown ? 1 : 2);

	ret = platform_device_register(&gpio_mouse_device);
	if (ret < 0) {
		pr_err(DRVNAME":    platform_device_register() returned %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit gpio_mouse_device_exit(void)
{
	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);

	platform_device_unregister(&gpio_mouse_device);
}

module_init(gpio_mouse_device_init);
module_exit(gpio_mouse_device_exit);

MODULE_DESCRIPTION("Adds a gpio_mouse device");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
