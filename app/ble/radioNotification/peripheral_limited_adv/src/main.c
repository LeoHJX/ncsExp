/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Peripheral example
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <zephyr.h>


#include <mpsl_radio_notification.h>
#include <mpsl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);


#define BLE_INT                         (0x140) // N * 0.625. corresponds to dec. 320; 320*0.625 = 200ms
#define BLE_ADV_TIMEOUT                 (80)  // N * 10ms for advertiser timeout
#define BLE_ADV_EVENTS                  (3)

static struct k_work start_advertising_worker;

static struct bt_le_ext_adv *adv;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0d, 0x18, 0x0f, 0x18, 0x0a, 0x18),
};

#if defined(CONFIG_BT_PERIPHERAL)

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_INF("Connection failed (err %d)\n", conn_err);
		return;
	}

	err = bt_conn_get_info(conn, &info);

	if (err) {
		LOG_INF("Failed to get connection info\n");
	} else {
		const struct bt_conn_le_phy_info *phy_info;
		phy_info = info.le.phy;

		LOG_INF("Connected: %s, tx_phy %u, rx_phy %u\n",
		       addr, phy_info->tx_phy, phy_info->rx_phy);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)\n", reason);

	k_work_submit(&start_advertising_worker);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	
};

#endif /* defined(CONFIG_BT_PERIPHERAL) */


static void adv_sent(struct bt_le_ext_adv *instance,
		     struct bt_le_ext_adv_sent_info *info)
{	
	LOG_INF("Advertising stopped. num_sent: %d ", info->num_sent);
}

static int create_advertising(void)
{
	int err;
	struct bt_le_adv_param param =
		BT_LE_ADV_PARAM_INIT(
				     BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_NAME,
				     BLE_INT,
				     BLE_INT,
				     NULL);

    static const struct bt_le_ext_adv_cb adv_cb = {
		.sent = adv_sent,
	};

	err = bt_le_ext_adv_create(&param, &adv_cb, &adv);
	if (err) {
		LOG_INF("Failed to create advertiser set (%d)\n", err);
		return err;
	}

	LOG_INF("Created adv: %p\n", adv);

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_INF("Failed to set advertising data (%d)\n", err);
		return err;
	}

	return 0;
}

static void start_advertising(struct k_work *item)
{
	int err;

	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_PARAM(BLE_ADV_TIMEOUT, BLE_ADV_EVENTS));
	if (err) {
		LOG_INF("Failed to start advertising set (%d)\n", err);
		return;
	}

	LOG_INF("Advertiser %p set started\n", adv);
}

static void bt_ready(void)
{
	int err = 0;

	LOG_INF("Bluetooth initialized\n");

	k_work_init(&start_advertising_worker, start_advertising);

	err = create_advertising();
	if (err) {
		LOG_INF("Advertising failed to create (err %d)\n", err);
		return;
	}

	k_work_submit(&start_advertising_worker);
}

void radio_handler(const void *context)
{

	LOG_INF("radio_handler");

}

void main(void)
{
	int err;

	LOG_INF("Main start");

	err = bt_enable(NULL);
	if (err) {
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return;
	}

	err = mpsl_radio_notification_cfg_set(MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_ACTIVE,MPSL_RADIO_NOTIFICATION_DISTANCE_5500US,SWI1_IRQn);
	if (err) {
		LOG_INF("mpsl_radio_notification_cfg_set failed (err %d)\n", err);
		return;
	}

	IRQ_CONNECT(SWI1_IRQn, 7,
		radio_handler, NULL, 0);
	irq_enable(SWI1_IRQn);
	

	bt_ready();

	bt_conn_cb_register(&conn_callbacks);
}
