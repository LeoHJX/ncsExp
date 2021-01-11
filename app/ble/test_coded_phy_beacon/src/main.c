/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include <devicetree.h>
#include <drivers/gpio.h>

#include <bluetooth/uuid.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define ADV_VIA_CODED_PHY 1

#define ADV_TIME_MS 200
#define ADV_INTERVAL_MS 2000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS DT_GPIO_FLAGS(LED0_NODE, gpios)

#define ADV_MF_DATA_LEN 7 /* make sure not overfill .  */
#define MANUFACTURE_ID 0x0059
uint8_t g_adv_mf_data[ADV_MF_DATA_LEN];

static struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, g_adv_mf_data, ADV_MF_DATA_LEN),
#if 0 /*  changed to counting */
	BT_DATA_BYTES(
			BT_DATA_MANUFACTURER_DATA, 0x59, 0x00, 'H', 'e', 'l', 'l', 'o',
			'w', ' ', 'w', 'o', 'r', 'l', 'd'), /* Nordic company id, please change this to your company ID */

#endif
};
static struct bt_le_adv_param *adv_param =
	BT_LE_ADV_PARAM(
#if 0
        /* BT_LE_ADV_OPT_USE_IDENTITY | */ 
#else
        BT_LE_ADV_OPT_ANONYMOUS |
#endif
        BT_LE_ADV_OPT_NO_2M | BT_LE_ADV_OPT_USE_TX_POWER

#if ADV_VIA_CODED_PHY
				| BT_LE_ADV_OPT_CODED | BT_LE_ADV_OPT_EXT_ADV
#endif
			,
			BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2,
			NULL);
/*  fill up the manufacture data and return the length */

void fill_mf_ad_pkg(void)
{
	g_adv_mf_data[0] = MANUFACTURE_ID & 0xFF;
	g_adv_mf_data[1] = 0xFF & (MANUFACTURE_ID >> 8);
	g_adv_mf_data[2] = 0xaa;
	g_adv_mf_data[3] = 0x55;
	g_adv_mf_data[4]++; /*   */
	g_adv_mf_data[5] = 0x55;
	g_adv_mf_data[6] = 0xaa;
}

void main(void)
{
	struct bt_le_ext_adv *ext_adv;

	const struct device *dev;
	bool led_is_on = true;
	int err;

	printk("Starting Beacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	dev = device_get_binding(LED0);
	if (dev == NULL) {
		return;
	}

	err = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (err < 0) {
		return;
	}

	err = bt_le_ext_adv_create(adv_param, NULL, &ext_adv);
	if (err) {
		printk("Fail to create ext advertising (err %d)\n", err);
		return;
	}
        ext_adv->tx_power = 8;
	k_sleep(K_MSEC(100)); /* add some delay, before start */
	while (1) {
		/* Advertising data */

		led_is_on = true;
		gpio_pin_set(dev, PIN, (int)led_is_on);

		// Start ext advertising
		fill_mf_ad_pkg();
                
		err = bt_le_ext_adv_set_data(ext_adv, ad, ARRAY_SIZE(ad), NULL,
					     0);
		if (err) {
			printk("Fail to set ext advertising (err %d)\n", err);
			return;
		}

		err = bt_le_ext_adv_start(ext_adv, NULL);
		if (err) {
			printk("Ext advertising failed to start (err %d)\n",
			       err);
			return;
		}

		k_sleep(K_MSEC(ADV_TIME_MS));

		led_is_on = false;
		gpio_pin_set(dev, PIN, (int)led_is_on);

		// Stop ext advertising
		err = bt_le_ext_adv_stop(ext_adv);
		if (err) {
			printk("Advertising failed to stop (err %d)\n", err);
			return;
		}
		k_sleep(K_MSEC(ADV_INTERVAL_MS));
	}
}
