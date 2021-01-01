/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @brief Nordic Battery Service Client sample
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <inttypes.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/bas_client.h>
#include <dk_buttons_and_leds.h>

#include <settings/settings.h>

#include "app_hr.h"

/**
 * Button to read the battery value
 */
#define KEY_READVAL_MASK DK_BTN1_MSK

#define BAS_READ_VALUE_INTERVAL (10 * MSEC_PER_SEC)

#define NUM_CONN_MAX    10


static struct bt_conn *curr_conn[NUM_CONN_MAX];
static struct bt_bas_client bas[NUM_CONN_MAX];

static void notify_battery_level_cb(struct bt_bas_client *bas,
				    uint8_t battery_level);

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

static struct bt_bas_client *find_bas_client(struct bt_conn *conn)
{
	int i;
	struct bt_bas_client *bas_mapped = NULL;

	if (!conn) {
		return NULL;
	}

	for (i = 0; i < NUM_CONN_MAX; i++) {
		if (curr_conn[i] == conn) {
			bas_mapped = bas + i;
			break;
		}
	}

	return bas_mapped;
}

static void scan_start(void)
{
	int err;

	/* This demo doesn't require active scan */
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	printk("Filters matched. Address: %s connectable: %s\n",
		addr, connectable ? "yes" : "no");
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	printk("Connecting failed\n");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	struct bt_conn **conn_room;

	conn_room = find_curr_conn(NULL);
	if (conn_room) {
		*conn_room = bt_conn_ref(conn);
	}
}

static void scan_filter_no_match(struct bt_scan_device_info *device_info,
				 bool connectable)
{
	int err;
	struct bt_conn *conn;
	char addr[BT_ADDR_LE_STR_LEN];

	if (device_info->recv_info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_addr_le_to_str(device_info->recv_info->addr, addr,
				  sizeof(addr));
		printk("Direct advertising received from %s\n", addr);

		struct bt_conn **conn_room;

		conn_room = find_curr_conn(NULL);
		if (conn_room) {
			bt_scan_stop();

			err = bt_conn_le_create(device_info->recv_info->addr,
						BT_CONN_LE_CREATE_CONN,
						device_info->conn_param, &conn);

			if (!err) {
				*conn_room = bt_conn_ref(conn);
				bt_conn_unref(conn);
			}

			conn_room = find_curr_conn(NULL);
			if (conn_room) {

			}
		}
	}
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match,
		scan_connecting_error, scan_connecting);

static void discovery_completed_cb(struct bt_gatt_dm *dm,
				   void *context)
{
	int err;

	printk("The discovery procedure succeeded\n");

	bt_gatt_dm_data_print(dm);

	struct bt_bas_client *bas = (struct bt_bas_client *)context;

	err = bt_bas_handles_assign(dm, bas);
	if (err) {
		printk("Could not init BAS client object, error: %d\n", err);
	}

	if (bt_bas_notify_supported(bas)) {
		err = bt_bas_subscribe_battery_level(bas,
						     notify_battery_level_cb);
		if (err) {
			printk("Cannot subscribe to BAS value notification "
				"(err: %d)\n", err);
			/* Continue anyway */
		}
	} else {
		err = bt_bas_start_per_read_battery_level(
			bas, BAS_READ_VALUE_INTERVAL, notify_battery_level_cb);
		if (err) {
			printk("Could not start periodic read of BAS value\n");
		}
	}

	err = bt_gatt_dm_data_release(dm);
	if (err) {
		printk("Could not release the discovery data, error "
		       "code: %d\n", err);
	}
}

static void discovery_service_not_found_cb(struct bt_conn *conn,
					   void *context)
{
	printk("The service could not be found during the discovery\n");
}

static void discovery_error_found_cb(struct bt_conn *conn,
				     int err,
				     void *context)
{
	printk("The discovery procedure failed with %d\n", err);
}

static struct bt_gatt_dm_cb discovery_cb = {
	.completed = discovery_completed_cb,
	.service_not_found = discovery_service_not_found_cb,
	.error_found = discovery_error_found_cb,
};

static void gatt_discover(struct bt_conn *conn)
{
	int err;
	struct bt_conn **conn_found;
	struct bt_bas_client *bas;

	conn_found = find_curr_conn(conn);
	if (!conn_found) {
		return;
	}

	bas = find_bas_client(conn);

	err = bt_gatt_dm_start(conn, BT_UUID_BAS, &discovery_cb, bas);
	if (err) {
		printk("Could not start the discovery procedure, error "
		       "code: %d\n", err);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	struct bt_conn_info info;
	struct bt_conn **conn_room;

	err = bt_conn_get_info(conn, &info);
	if (err) {
		return;
	} else if (info.role == BT_CONN_ROLE_SLAVE) {
		bt_hr_connected(conn, conn_err);
		return;
	}

	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);

		conn_room = find_curr_conn(conn);
		if (conn_room) {
			bt_conn_unref(*conn_room);
			*conn_room = NULL;

			scan_start();
		} else {
			printk("No room to trace connection!\n");
		}

		return;
	}

	printk("Connected: %s\n", addr);

	err = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (err) {
		printk("Failed to set security: %d\n", err);

		gatt_discover(conn);
	}

	conn_room = find_curr_conn(NULL);
	if (conn_room) {
		scan_start();
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;
	struct bt_conn_info info;

	err = bt_conn_get_info(conn, &info);
	if (err) {
		return;
	} else if (info.role == BT_CONN_ROLE_SLAVE) {
		bt_hr_disconnected(conn, reason);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason %u)\n", addr, reason);

	struct bt_conn **conn_found;

	conn_found = find_curr_conn(conn);
	if (conn_found) {
		bt_conn_unref(*conn_found);
		*conn_found = NULL;
	}

	scan_start();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d\n", addr, level,
			err);
	}

	gatt_discover(conn);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed
};

static void scan_init(void)
{
	int err;

	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
		.scan_param = NULL,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_BAS);
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
	}
}

static void notify_battery_level_cb(struct bt_bas_client *bas,
				    uint8_t battery_level)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(bt_bas_conn(bas)),
			  addr, sizeof(addr));
	if (battery_level == BT_BAS_VAL_INVALID) {
		printk("[%s] Battery notification aborted\n", addr);
	} else {
		printk("[%s] Battery notification: %"PRIu8"%%\n",
		       addr, battery_level);
	}
}

static void read_battery_level_cb(struct bt_bas_client *bas,
				  uint8_t battery_level,
				  int err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(bt_bas_conn(bas)),
			  addr, sizeof(addr));
	if (err) {
		printk("[%s] Battery read ERROR: %d\n", addr, err);
		return;
	}

	printk("[%s] Battery read: %"PRIu8"%%\n", addr, battery_level);
}

static void button_readval(void)
{
	int err;
	int i;

	printk("Reading BAS value:\n");

	for (i = 0; i < NUM_CONN_MAX; i++) {
		if (curr_conn[i]) {
			err = bt_bas_read_battery_level(bas + i, read_battery_level_cb);
			if (err) {
				printk("BAS read call error: %d\n", err);
			}
		}
	}
}


static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;

	if (button & KEY_READVAL_MASK) {
		button_readval();
	}
}


static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}


static void pairing_confirm(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	bt_conn_auth_pairing_confirm(conn);

	printk("Pairing confirmed: %s\n", addr);
}


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}


static struct bt_conn_auth_cb conn_auth_callbacks = {
	.cancel = auth_cancel,
	.pairing_confirm = pairing_confirm,
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};


void main(void)
{
	int err;

	printk("Starting Bluetooth Central BAS example\n");

	int i;

	for (i = 0; i < NUM_CONN_MAX; i++) {
		bt_bas_client_init(bas + i);
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	scan_init();
	bt_conn_cb_register(&conn_callbacks);

	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		printk("Failed to register authorization callbacks.\n");
		return;
	}

	err = dk_buttons_init(button_handler);
	if (err) {
		printk("Failed to initialize buttons (err %d)\n", err);
		return;
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");

	bt_hr_start();

	bt_hr_main_loop();
}
