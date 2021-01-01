/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>
#include <bluetooth/services/hrs.h>

#include "app_hr.h"

#define NUM_CONN_MAX    10

static struct bt_conn *curr_conn[NUM_CONN_MAX];

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_DIS_VAL))
};

static struct bt_conn **find_curr_conn(struct bt_conn *conn)
{
	int i;
	struct bt_conn **conn_found = NULL;

	for (i = 0; i < NUM_CONN_MAX; i++) {
		if (curr_conn[i] == conn) {
			conn_found = curr_conn + i;
			break;
		}
	}

	return conn_found;
}

void bt_hr_connected(struct bt_conn *conn, uint8_t err)
{
	struct bt_conn **conn_room;

	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		conn_room = find_curr_conn(NULL);
		if (conn_room) {
			*conn_room = bt_conn_ref(conn);
			printk("Connected\n");
		} else {
			printk("No room to trace connection!\n");
		}
	}
}

void bt_hr_disconnected(struct bt_conn *conn, uint8_t reason)
{
	struct bt_conn **conn_found;

	printk("Disconnected (reason 0x%02x)\n", reason);

	conn_found = find_curr_conn(conn);
	if (conn_found) {
		bt_conn_unref(*conn_found);
		*conn_found = NULL;
	}
}

void bt_hr_start(void)
{
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

static void hrs_notify(void)
{
	static uint8_t heartrate = 90U;

	/* Heartrate measurements simulation */
	heartrate++;
	if (heartrate == 160U) {
		heartrate = 90U;
	}

	bt_hrs_notify(heartrate);
}

void bt_hr_main_loop(void)
{
	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1) {
		k_sleep(K_SECONDS(1));

		/* Heartrate measurements simulation */
		hrs_notify();

		/* Battery level simulation */
		bas_notify();
	}
}
