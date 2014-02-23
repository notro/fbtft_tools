/*
 * Adds a gpio_backlight device
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
#include <linux/platform_device.h>
#include <linux/platform_data/gpio_backlight.h>

#define DRVNAME "gpio_backlight_device"

static int gpio = -1;
module_param(gpio, int, 0);
MODULE_PARM_DESC(gpio, "GPIO controlling the backlight");

static int def_value = 1;
module_param(def_value, int, 0);
MODULE_PARM_DESC(def_value, "Default value");

static bool active_low;
module_param(active_low, bool, 0);
MODULE_PARM_DESC(active_low, "GPIO is active low");

static bool verbose;
module_param(verbose, bool, 0);


static struct gpio_backlight_platform_data pdata = {
	.name = DRVNAME,
};

static void pdev_release(struct device *dev)
{
	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);
}

static struct platform_device gpio_backlight_device = {
	.name = "gpio-backlight",
	.dev  = {
		.release = pdev_release,
		.platform_data = &pdata,
	},
};


static int __init gpio_backlight_device_init(void)
{
	int ret;

	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);

	if (gpio < 0) {
#ifdef MODULE
		pr_err(DRVNAME": missing module parameter: 'gpio'\n");
		return -EINVAL;
#else
		return 0;
#endif
	}

	pdata.gpio = gpio;
	pdata.def_value = def_value;
	pdata.active_low = active_low;

	ret = platform_device_register(&gpio_backlight_device);
	if (ret < 0) {
		pr_err(DRVNAME": platform_device_register() returned %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit gpio_backlight_device_exit(void)
{
	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);

	platform_device_unregister(&gpio_backlight_device);
}

module_init(gpio_backlight_device_init);
module_exit(gpio_backlight_device_exit);

MODULE_DESCRIPTION("Adds a gpio_backlight device");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
