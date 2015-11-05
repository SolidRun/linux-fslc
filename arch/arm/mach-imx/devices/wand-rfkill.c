/*
 * arch/arm/mach-imx/devices/wand-rfkill.c
 *
 * Copyright (C) 2013 Vladimir Ermakov <vooon341@gmail.com>
 *
 * based on net/rfkill/rfkill-gpio.c
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

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#define REVISION_B0			0
#define REVISION_C1			1

#define GPIO_COUNT			8
#define GPIO_WIFI_REG			0
#define GPIO_WIFI_REF			1
#define GPIO_WIFI_RST			2
#define GPIO_WIFI_WAKE			3
#define GPIO_WIFI_HOST_WAKE		4
#define GPIO_BT_RST 			5
#define GPIO_BT_WAKE 			6
#define GPIO_BT_HOST_WAKE		7

#define DEVICE_COUNT			2
#define DEVICE_WIFI 			0
#define DEVICE_BLUETOOTH		1

struct wand_rfkill_data {
	unsigned int *gpios;
	struct wand_rfkill_dev *devices;
	int revision;
};

struct wand_rfkill_dev {
	struct rfkill *rfkill_dev;
	unsigned int *shutdown_gpios;
	unsigned int shutdown_gpios_count;
	const char *shutdown_name;
};

static int wand_rfkill_dev_set_block(void *data, bool blocked)
{
	int i;
	struct wand_rfkill_dev *device = data;

	pr_debug("wandboard-rfkill: dev %s: set block %d\n", device->shutdown_name, blocked);

	if (blocked) {
		for(i = 0; i < device->shutdown_gpios_count;i++) {
			gpio_set_value(device->shutdown_gpios[i], 0);
		}
	} else {
		for(i = device->shutdown_gpios_count - 1; i > 0;i--) {
			gpio_set_value(device->shutdown_gpios[i], 1);
			msleep(10);
		}
	}

	return 0;
}

static const struct rfkill_ops wand_rfkill_ops = {
	.set_block = wand_rfkill_dev_set_block,
};

static int wand_rfkill_wifi_probe(struct device *dev,
	struct wand_rfkill_data *rfkill)
	{
	int ret;

	rfkill->devices[DEVICE_WIFI].shutdown_gpios = kzalloc(sizeof(unsigned int) * 4, GFP_KERNEL);
	if(!rfkill->devices[DEVICE_WIFI].shutdown_gpios)
		return -ENOMEM;

	dev_info(dev, "initialize wifi chip\n");

	/* Enable the WiFi part of the chip */
	/* Enable power */
	gpio_set_value(rfkill->gpios[GPIO_WIFI_REF], 1);
	gpio_set_value(rfkill->gpios[GPIO_WIFI_REG], 1);

	/* Enable reset and set wake */
	gpio_set_value(rfkill->gpios[GPIO_WIFI_RST], 1);
	gpio_set_value(rfkill->gpios[GPIO_WIFI_WAKE], 1);

	rfkill->devices[DEVICE_WIFI].shutdown_name = "wifi_shutdown";
	rfkill->devices[DEVICE_WIFI].shutdown_gpios[0] = rfkill->gpios[GPIO_WIFI_WAKE];
	rfkill->devices[DEVICE_WIFI].shutdown_gpios[1] = rfkill->gpios[GPIO_WIFI_RST];
	rfkill->devices[DEVICE_WIFI].shutdown_gpios[2] = rfkill->gpios[GPIO_WIFI_REG];
	rfkill->devices[DEVICE_WIFI].shutdown_gpios[3] = rfkill->gpios[GPIO_WIFI_REF];
	rfkill->devices[DEVICE_WIFI].shutdown_gpios_count = 4;

	rfkill->devices[DEVICE_WIFI].rfkill_dev = rfkill_alloc("wifi-rfkill", dev,
		RFKILL_TYPE_WLAN, &wand_rfkill_ops, &(rfkill->devices[DEVICE_WIFI]));
	if (!rfkill->devices[DEVICE_WIFI].rfkill_dev)
		return -ENOMEM;

	ret = rfkill_register(rfkill->devices[DEVICE_WIFI].rfkill_dev);
	if (ret < 0)
		goto wifi_fail_unregister;

	dev_info(dev, "wifi-rfkill registered.\n");

	return 0;

wifi_fail_unregister:
	rfkill_destroy(rfkill->devices[DEVICE_WIFI].rfkill_dev);

	return ret;
}

static int wand_rfkill_bt_probe(struct device *dev,
		struct wand_rfkill_data *rfkill)
{
	int ret;

	rfkill->devices[DEVICE_BLUETOOTH].shutdown_gpios = kzalloc(sizeof(unsigned int) * 2, GFP_KERNEL);
	if(!rfkill->devices[DEVICE_BLUETOOTH].shutdown_gpios)
		return -ENOMEM;

	dev_info(dev, "initialize bluetooth chip\n");
    /* Enable the Bluetooth part of the chip */
    /* Enable reset and set wake */
	gpio_set_value(rfkill->gpios[GPIO_BT_RST], 1);
	gpio_set_value(rfkill->gpios[GPIO_BT_WAKE], 1);

	rfkill->devices[DEVICE_BLUETOOTH].shutdown_name = "bluetooth_shutdown";
	rfkill->devices[DEVICE_BLUETOOTH].shutdown_gpios[0] = rfkill->gpios[GPIO_BT_WAKE];
	rfkill->devices[DEVICE_BLUETOOTH].shutdown_gpios[1] = rfkill->gpios[GPIO_BT_RST];
	rfkill->devices[DEVICE_BLUETOOTH].shutdown_gpios_count = 2;

	rfkill->devices[DEVICE_BLUETOOTH].rfkill_dev = rfkill_alloc("bluetooth-rfkill", dev,
			RFKILL_TYPE_BLUETOOTH, &wand_rfkill_ops, &(rfkill->devices[DEVICE_BLUETOOTH]));
	if(!rfkill->devices[DEVICE_BLUETOOTH].rfkill_dev)
		return -ENOMEM;

	ret = rfkill_register(rfkill->devices[DEVICE_BLUETOOTH].rfkill_dev);
	if(ret < 0)
		goto bt_fail_unregister;

	dev_info(dev, "bluetooth-rfkill registered\n");

	return 0;

bt_fail_unregister:
	rfkill_destroy(rfkill->devices[DEVICE_BLUETOOTH].rfkill_dev);

	return ret;
}

static int wand_rfkill_probe(struct platform_device *pdev)
{
	struct wand_rfkill_data *rfkill;
	struct pinctrl *pinctrl;
	int ret;
	unsigned int wand_rev_gpio;
	int wand_rev;
	int i;

	dev_info(&pdev->dev, "Wandboard rfkill initialization\n");

	if(!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no device tree node\n");
		return -ENODEV;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if(IS_ERR(pinctrl)) {
		int ret = PTR_ERR(pinctrl);
		dev_err(&pdev->dev, "failed to get default pinctrl: %d\n", ret);
		return ret;
	}

	/* GPIO for detecting C1 revision of Wandboard */
	wand_rev_gpio = of_get_named_gpio(pdev->dev.of_node, "wand-rev-gpio", 0);
	if(!gpio_is_valid(wand_rev_gpio)) {
		dev_err(&pdev->dev, "incorrect Wandboard revision check gpio (%d)\n",
				wand_rev_gpio);
		return -EINVAL;
	}

	gpio_request(wand_rev_gpio, "wand-rev-gpio");
	dev_info(&pdev->dev, "initialized Wandboard revision check gpio (%d)\n",
			wand_rev_gpio);
	gpio_direction_input(wand_rev_gpio);


	rfkill = kzalloc(sizeof(struct wand_rfkill_data), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	/* Check Wandboard revision */
	wand_rev = gpio_get_value(wand_rev_gpio);
	if(wand_rev) {
		dev_info(&pdev->dev,"wandboard is rev C1\n");
		rfkill->revision = REVISION_C1;
	} else {
		dev_info(&pdev->dev,"wandboard is rev B0\n");
		rfkill->revision = REVISION_B0;
	}

	gpio_free(wand_rev_gpio);

	rfkill->gpios = kzalloc(sizeof(unsigned int) * GPIO_COUNT, GFP_KERNEL);
	if(!rfkill->gpios) {
		ret = -ENOMEM;
		goto fail_free_rfkill;
	}

	/* Initialize WiFi GPIOs */
	rfkill->gpios[GPIO_WIFI_REG] = of_get_named_gpio(pdev->dev.of_node, "wifi-reg-on", 0);
	rfkill->gpios[GPIO_WIFI_WAKE] = of_get_named_gpio(pdev->dev.of_node, "wifi-wake", 0);
	rfkill->gpios[GPIO_WIFI_HOST_WAKE] = of_get_named_gpio(pdev->dev.of_node, "wifi-host-wake", 0);

	if(rfkill->revision == REVISION_C1) {
		rfkill->gpios[GPIO_WIFI_REF] = of_get_named_gpio(pdev->dev.of_node, "wifi-ref-on-revc1", 0);
		rfkill->gpios[GPIO_WIFI_RST] = of_get_named_gpio(pdev->dev.of_node, "wifi-rst-n-revc1", 0);
	} else if(rfkill->revision == REVISION_B0) {
		rfkill->gpios[GPIO_WIFI_REF] = of_get_named_gpio(pdev->dev.of_node, "wifi-ref-on", 0);
		rfkill->gpios[GPIO_WIFI_RST] = of_get_named_gpio(pdev->dev.of_node, "wifi-rst-n", 0);
	}

	if(!gpio_is_valid(rfkill->gpios[GPIO_WIFI_REG]) || !gpio_is_valid(rfkill->gpios[GPIO_WIFI_WAKE]) ||
		!gpio_is_valid(rfkill->gpios[GPIO_WIFI_HOST_WAKE]) || !gpio_is_valid(rfkill->gpios[GPIO_WIFI_REF]) ||
		!gpio_is_valid(GPIO_WIFI_RST)) {

		dev_err(&pdev->dev, "incorrect wifi gpios (%d %d %d %d %d)\n",
				rfkill->gpios[GPIO_WIFI_REG], rfkill->gpios[GPIO_WIFI_WAKE],
				rfkill->gpios[GPIO_WIFI_HOST_WAKE], rfkill->gpios[GPIO_WIFI_REF],
				rfkill->gpios[GPIO_WIFI_RST]);
		ret = -EINVAL;
		goto fail_free_gpios;
	}

	/* Initialize BT GPIOs */
	if(rfkill->revision == REVISION_C1) {
		rfkill->gpios[GPIO_BT_RST] = of_get_named_gpio(pdev->dev.of_node, "bluetooth-on-revc1", 0);
		rfkill->gpios[GPIO_BT_WAKE] = of_get_named_gpio(pdev->dev.of_node, "bluetooth-wake-revc1", 0);
		rfkill->gpios[GPIO_BT_HOST_WAKE] = of_get_named_gpio(pdev->dev.of_node, "bluetooth-host-wake-revc1", 0);
	} else if(rfkill->revision == REVISION_B0) {
		rfkill->gpios[GPIO_BT_RST] = of_get_named_gpio(pdev->dev.of_node, "bluetooth-on", 0);
		rfkill->gpios[GPIO_BT_WAKE] = of_get_named_gpio(pdev->dev.of_node, "bluetooth-wake", 0);
		rfkill->gpios[GPIO_BT_HOST_WAKE] = of_get_named_gpio(pdev->dev.of_node, "bluetooth-host-wake", 0);
	}

	if(!gpio_is_valid(rfkill->gpios[GPIO_BT_RST]) || !gpio_is_valid(rfkill->gpios[GPIO_BT_RST]) ||
		!gpio_is_valid(rfkill->gpios[GPIO_BT_HOST_WAKE])) {

		dev_err(&pdev->dev, "incorrect bt gpios (%d) (%d) (%d)\n",
				rfkill->gpios[GPIO_BT_RST], rfkill->gpios[GPIO_BT_WAKE],
				rfkill->gpios[GPIO_BT_HOST_WAKE]);
		ret = -EINVAL;
		goto fail_free_gpios;
	}

	dev_info(&pdev->dev, "Resetting bcm4329/bcm4330\n");
	/* Initialize directions and set initial value to off, effectively resetting the chip */
	/* Reset both WAKE pins to signal that the host doesn't want wifi or bt to be active */
	gpio_request(rfkill->gpios[GPIO_WIFI_WAKE], "wl_wake");
	gpio_direction_output(rfkill->gpios[GPIO_WIFI_WAKE], 0);
	gpio_request(rfkill->gpios[GPIO_BT_WAKE], "bt_wake");
	gpio_direction_output(rfkill->gpios[GPIO_BT_WAKE], 0);

	/* Reset both RST pins to disable WiFi and BT */
	gpio_request(rfkill->gpios[GPIO_WIFI_RST], "wl_rst");
	gpio_direction_output(rfkill->gpios[GPIO_WIFI_RST], 0);
	gpio_request(rfkill->gpios[GPIO_BT_RST], "bt_rst");
	gpio_direction_output(rfkill->gpios[GPIO_BT_RST], 0);

	/* Reset WL_REG to remove power from the internal regulators */
	gpio_request(rfkill->gpios[GPIO_WIFI_REG], "wl_reg");
	gpio_direction_output(rfkill->gpios[GPIO_WIFI_REG], 0);

	/* Also reset WL_REF_ON ("Wifi Power Enable" according to dts) */
	gpio_request(rfkill->gpios[GPIO_WIFI_REF], "wl_ref");
	gpio_direction_output(rfkill->gpios[GPIO_WIFI_REF], 0);

	/* Initialize both HOST_WAKE pins to input */
	gpio_request(rfkill->gpios[GPIO_WIFI_HOST_WAKE], "wl_host_wake");
	gpio_direction_input(rfkill->gpios[GPIO_WIFI_HOST_WAKE]);
	gpio_request(rfkill->gpios[GPIO_BT_HOST_WAKE], "bt_host_wake");
	gpio_direction_input(rfkill->gpios[GPIO_BT_HOST_WAKE]);

	/* Sleep a generous amount of time to reset the chip */
	msleep(100);

	rfkill->devices = kzalloc(sizeof(struct wand_rfkill_dev) * DEVICE_COUNT, GFP_KERNEL);
	if(!rfkill->devices) {
		ret = -ENOMEM;
		goto fail_free_requested_gpios;
	}


	/* Setup the WiFi part */
	ret = wand_rfkill_wifi_probe(&pdev->dev, rfkill);
	if(ret < 0)
		goto fail_free_devices;

	/* Setup the Bluetooth part */
	ret = wand_rfkill_bt_probe(&pdev->dev, rfkill);
	if(ret < 0)
		goto fail_unregister_device_wifi;

	platform_set_drvdata(pdev, rfkill);

	return 0;

fail_unregister_device_wifi:
	if(rfkill->devices[DEVICE_WIFI].rfkill_dev) {
		rfkill_unregister(rfkill->devices[DEVICE_WIFI].rfkill_dev);
		rfkill_destroy(rfkill->devices[DEVICE_WIFI].rfkill_dev);
	}
	kfree(rfkill->devices[DEVICE_WIFI].shutdown_gpios);
fail_free_devices:
	kfree(rfkill->devices);
fail_free_requested_gpios:
	for(i = 0; i < GPIO_COUNT; i++) {
		gpio_free(rfkill->gpios[i]);
	}
fail_free_gpios:
	kfree(rfkill->gpios);
fail_free_rfkill:
	kfree(rfkill);

	return ret;
}

static int wand_rfkill_remove(struct platform_device *pdev)
{
	struct wand_rfkill_data *rfkill = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Module unloading\n");

	if(rfkill) {
		if(rfkill->devices) {
			int i;
			for(i = 0; i < DEVICE_COUNT; i++) {
				if(rfkill->devices[i].rfkill_dev) {
					rfkill_unregister(rfkill->devices[i].rfkill_dev);
					rfkill_destroy(rfkill->devices[i].rfkill_dev);
				}
				if(rfkill->devices[i].shutdown_gpios) {
					kfree(rfkill->devices[i].shutdown_gpios);
				}
			}
			kfree(rfkill->devices);
		}

		if(rfkill->gpios) {
			int i;
			for(i = 0; i < GPIO_COUNT; i++) {
				if(gpio_is_valid(rfkill->gpios[i]))
					gpio_free(rfkill->gpios[i]);
			}
			kfree(rfkill->gpios);
		}

		kfree(rfkill);
	}

	return 0;
}

static struct of_device_id wand_rfkill_match[] = {
	{ .compatible = "wand,imx6q-wandboard-rfkill", },
	{ .compatible = "wand,imx6dl-wandboard-rfkill", },
	{ .compatible = "wand,imx6qdl-wandboard-rfkill", },
	{}
};

static struct platform_driver wand_rfkill_driver = {
	.driver = {
		.name = "wandboard-rfkill",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(wand_rfkill_match),
	},
	.probe = wand_rfkill_probe,
	.remove = wand_rfkill_remove
};

module_platform_driver(wand_rfkill_driver);

MODULE_AUTHOR("Vladimir Ermakov <vooon341@gmail.com>");
MODULE_DESCRIPTION("Wandboard rfkill driver");
MODULE_LICENSE("GPL v2");
