/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

void bt_hr_connected(struct bt_conn *conn, uint8_t err);

void bt_hr_disconnected(struct bt_conn *conn, uint8_t reason);

void bt_hr_start(void);

void bt_hr_main_loop(void);
