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
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>

#include <devicetree.h>
#include <drivers/gpio.h>

#include <bluetooth/uuid.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define ADV_VIA_CODED_PHY 1

#define ADV_TIME_MS 1000
#define ADV_INTERVAL_MS 200

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS DT_GPIO_FLAGS(LED0_NODE, gpios)

#define ADV_MF_DATA_LEN 50 /* make sure not overfill .  */
#define MANUFACTURE_ID 0x0059

#define ADV_DATA_FIXED 0

#define TX_POWER_CHANGE 1

#if TX_POWER_CHANGE
/* TX power list
nRF52832 : -40, -20, -16, -12, -8, -4, 0, 4 
nRF52810 : -40, -20, -16, -12, -8, -4, 0, 4 
nRF52840 : -40, -20, -16, -12, -8, -4, 0, 4, 8 

*/

#define BLE_TX_POWER_LEVEL    (-4)

static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				sizeof(*cp));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = tx_pwr_lvl;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		uint8_t reason = rsp ?
			((struct bt_hci_rp_vs_write_tx_power_level *)
			  rsp->data)->status : 0;
		printk("Set Tx power err: %d reason 0x%02x\n", err, reason);
		return;
	}

	rp = (void *)rsp->data;
	printk("Actual Tx Power: %d\n", rp->selected_tx_power);

	net_buf_unref(rsp);
}

#endif


uint8_t g_adv_mf_data[ADV_MF_DATA_LEN];


static struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, g_adv_mf_data, ADV_MF_DATA_LEN),
};



static struct bt_le_adv_param *adv_param =
	BT_LE_ADV_PARAM(
#if 1
         BT_LE_ADV_OPT_USE_IDENTITY |
#else
        BT_LE_ADV_OPT_ANONYMOUS |
#endif
        BT_LE_ADV_OPT_NO_2M | BT_LE_ADV_OPT_USE_TX_POWER

#if ADV_VIA_CODED_PHY
				| BT_LE_ADV_OPT_CODED | BT_LE_ADV_OPT_EXT_ADV
#endif
			,
			BT_GAP_ADV_FAST_INT_MIN_1, BT_GAP_ADV_FAST_INT_MAX_1,
			NULL);
/*  fill up the manufacture data and return the length */

void fill_mf_ad_pkg(void)
{
	g_adv_mf_data[0] = MANUFACTURE_ID & 0xFF;
	g_adv_mf_data[1] = 0xFF & (MANUFACTURE_ID >> 8);
#if !ADV_DATA_FIXED
	g_adv_mf_data[2] = 0xaa;
	g_adv_mf_data[3] = 0x55;
	g_adv_mf_data[4]++; /*   */
	g_adv_mf_data[5] = 0x55;
	g_adv_mf_data[6] = 0xaa;
#else
       strcpy(&(g_adv_mf_data[2]), "Hello World!!");
#endif
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

#if TX_POWER_CHANGE
        /* setup TX power */
		set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, BLE_TX_POWER_LEVEL);
#endif
	k_sleep(K_MSEC(100)); /* add some delay, before start */
	while (1) {
		/* Advertising data */

		led_is_on = true;
		gpio_pin_set(dev, PIN, (int)led_is_on);
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
