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

static void pdev_release(struct device *dev)
{
 /* Used to silence this message:  Device 'xxx' does not have a release() function, it is broken and must be fixed. */
}

static int __init gpio_mouse_device_init(void)
{
	int ret;

	if (verbose)
		pr_info("\n\n"DRVNAME": %s()\n", __func__);

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

	ret = platform_device_register(&gpio_mouse_device);
	if (ret < 0) {
		pr_err(DRVNAME":    platform_device_register() returned %d\n", ret);
		return ret;
	}

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
