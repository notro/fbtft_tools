/*
 * Adds a gpio_keys device
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
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DRVNAME "gpio_keys_device"

#define MAX_KEYS 64


static char *keys[MAX_KEYS] = { NULL, };
static int keys_num = 0;
module_param_array(keys, charp, &keys_num, 0);
MODULE_PARM_DESC(keys, "List of keys.");

static unsigned int poll_interval = 20;
module_param(poll_interval, int, 0);
MODULE_PARM_DESC(poll_interval, "polling interval in msecs - for polling driver only (default=20)");

static bool repeat = false;
module_param(repeat, bool, 0);
MODULE_PARM_DESC(repeat, "enable input subsystem auto repeat");

static bool polled = false;
module_param(polled, bool, 0);
MODULE_PARM_DESC(polled, "use polled driver: gpio_keys_polled");

static bool active_low = false;
module_param(active_low, bool, 0);
MODULE_PARM_DESC(active_low, "Set active_low=1 as default");

static unsigned int debounce_interval = 5;
module_param(debounce_interval, int, 0);
MODULE_PARM_DESC(debounce_interval, "Set default debounce_interval (default=5)");

static unsigned int type = EV_KEY;
module_param(type, int, 0);
MODULE_PARM_DESC(type, "Set default event type (default=EV_KEY)");

static bool pullup = false;
module_param(pullup, bool, 0);
MODULE_PARM_DESC(pulldown, "Enable internal pull up resistor for all (only on Raspberry Pi)");

static bool pulldown = false;
module_param(pulldown, bool, 0);
MODULE_PARM_DESC(pulldown, "Enable internal pull down resistor for all (only on Raspberry Pi)");

static unsigned int verbose = 0;
module_param(verbose, uint, 0);
MODULE_PARM_DESC(verbose, "0-3");


static struct gpio_keys_button gpio_keys_table[MAX_KEYS] = { };

static struct gpio_keys_platform_data pdata = {
        .buttons        = gpio_keys_table,
};

static void pdev_release(struct device *dev);

static struct platform_device gpio_keys_device = {
	.dev  = {
		.release = pdev_release,
		.platform_data = &pdata,
	},
};

#if defined(CONFIG_ARCH_BCM2708) || defined(CONFIG_ARCH_BCM2709)

//Pi or Pi2 architecture?
#ifdef CONFIG_ARCH_BCM2709
#define BCM2708_PERI_BASE       0x3F000000
#else
#define BCM2708_PERI_BASE       0x20000000
#endif

static void gpio_pull(unsigned pin, unsigned pud)
{
#define	GPIO_PUD     37
#define	GPIO_PUDCLK0 38
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
	u32 *gpio = ioremap(BCM2708_PERI_BASE + 0x200000, SZ_16K);

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
	int i;

	if (verbose > 1)
		pr_info(DRVNAME": %s(%d)\n", __func__, pud);

	for (i=0;i<keys_num;i++) {
		gpio_pull(gpio_keys_table[i].gpio, pud);
	}
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

static int get_next_keys_button_value(char **str, char *name, int default_value)
{
	char *p_val;
	int val = default_value;
	int ret;

	if (*str) {
		p_val = strsep(str, ":");

		if (verbose > 2) {
			printk("*str: %p,  tmp: %p\n", *str, p_val);
			if (*str)
				printk("*str: '%s'\n", *str);
			if (p_val)
				printk("p_val: '%s'\n", p_val);
		}

		if (p_val) {
			if (strlen(p_val) == 0) {
				val = default_value;
			} else {
				ret = kstrtoint(p_val, 10, &val);
				if (ret) {
					pr_err(DRVNAME":  could not parse number in keys parameter: '%s'\n", p_val);
					return -EINVAL;
				}
			}
		}
	}

	if (verbose)
		pr_info(DRVNAME":   %s = %d\n", name, val);

	return val;
}

static int __init gpio_keys_device_init(void)
{
	int ret, i;
	char *p;

	if (polled)
		gpio_keys_device.name = "gpio-keys-polled";
	else
		gpio_keys_device.name = "gpio-keys";

	if (pullup && pulldown) {
		pr_err(DRVNAME":  can't have both pullup and pulldown\n");
		return -EINVAL;
	}

	if (verbose) {
		pr_info("\n\n"DRVNAME": %s()\n", __func__);
		pr_info(DRVNAME":   driver: %s\n", gpio_keys_device.name);
		if (polled)
			pr_info(DRVNAME":   poll_interval = %d\n", poll_interval);
		pr_info(DRVNAME":   repeat = %s\n", repeat ? "yes" : "no");
	}

	if (verbose && (pullup || pulldown))
		pr_info(DRVNAME":   Internal pull resistor: %s\n", pullup ? "up" : "down");

	/* parse module parameter: keys */
	if (keys_num == 0) {
		pr_err(DRVNAME":  required 'keys' parameter missing\n");
		return -EINVAL;
	}
	if (keys_num > MAX_KEYS) {
		pr_err(DRVNAME":  keys parameter: exceeded max array size: %d\n", MAX_KEYS);
		return -EINVAL;
	}
	for (i=0;i<keys_num;i++) {
		if (strchr(keys[i], ':') == NULL) {
			pr_err(DRVNAME":  error missing ':' in keys parameter: %s\n", keys[i]);
			return -EINVAL;
		}
		p = keys[i];
		if (verbose)
			pr_info(DRVNAME": Key: '%s'\n", p);
		/* gpio */
		ret = get_next_keys_button_value(&p, "gpio", -1);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].gpio = ret;
		/* input event code (KEY_*, SW_*) */
		ret = get_next_keys_button_value(&p, "code", -1);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].code = ret;
		/* input event type (EV_KEY, EV_SW, EV_ABS) */
		ret = get_next_keys_button_value(&p, "type", type);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].type = ret;
		/* active_low */
		ret = get_next_keys_button_value(&p, "active_low", active_low);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].active_low = ret;
		/* debounce_interval */
		ret = get_next_keys_button_value(&p, "debounce_interval", debounce_interval);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].debounce_interval = ret;
		/* can_disable */
		ret = get_next_keys_button_value(&p, "can_disable", 0);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].can_disable = ret;
		/* axis value for EV_ABS */
		ret = get_next_keys_button_value(&p, "value", 0);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].value = ret;
		/* wakeup */
		ret = get_next_keys_button_value(&p, "wakeup", 0);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].wakeup = ret;
		/* irq */
		ret = get_next_keys_button_value(&p, "irq", 0);
		if (ret < 0)
			return -EINVAL;
		gpio_keys_table[i].irq = ret;

		if (p != NULL) {
			pr_err(DRVNAME":  unparsed part in keys parameter: '%s'\n", p);
			return -EINVAL;
		}
	}
	pdata.buttons        = gpio_keys_table;
	pdata.nbuttons       = keys_num;
	pdata.poll_interval  = poll_interval;

	if (pullup || pulldown)
		gpio_pull_all(pulldown ? 1 : 2);

	ret = platform_device_register(&gpio_keys_device);
	if (ret < 0) {
		pr_err(DRVNAME":    platform_device_register() returned %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit gpio_keys_device_exit(void)
{
	if (verbose)
		pr_info(DRVNAME": %s()\n", __func__);

	platform_device_unregister(&gpio_keys_device);
}

module_init(gpio_keys_device_init);
module_exit(gpio_keys_device_exit);

MODULE_DESCRIPTION("Adds a gpio_keys device");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
