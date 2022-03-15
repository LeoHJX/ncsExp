/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>

#include <console/console.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/iso.h>

#include "common.h"

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(iso_broadcast_main, LOG_LEVEL_DBG);




enum benchmark_role {
	ROLE_RECEIVER,
	ROLE_BROADCASTER,
	ROLE_QUIT
};

/*  mode: 1 - pa, 0 - lna */
#define ANT_SEL_PIN 6
#define TX_EN_PIN 7
#define RX_EN_PIN 5
#define PDN_EN_PIN	11 
#define MODE_PIN	4
#define CS_PIN 12
int init_pa_lna(int mode)
{
	const struct device *gpio1;
	int ret = 0;

	gpio1 = device_get_binding("GPIO_1");


	ret = gpio_pin_configure(gpio1, PDN_EN_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
	if (ret < 0) {
		return ret;
	}	
	ret = gpio_pin_configure(gpio1, CS_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
	if (ret < 0) {
		return ret;
	}	

	ret = gpio_pin_configure(gpio1, MODE_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_HIGH);
	if (ret < 0) {
		return ret;
	}	
	ret = gpio_pin_configure(gpio1, ANT_SEL_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_HIGH);
	if (ret < 0) {
		return ret;
	}
	if(mode == 1)
	{
		ret = gpio_pin_configure(gpio1, TX_EN_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
		if (ret < 0) {
			return ret;
		}
		ret = gpio_pin_configure(gpio1, RX_EN_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_HIGH);
		if (ret < 0) {
			return ret;
		}
	}
	else
	{
		ret = gpio_pin_configure(gpio1, TX_EN_PIN,  GPIO_OUTPUT_INACTIVE| GPIO_ACTIVE_HIGH);
		if (ret < 0) {
			return ret;
		}
		ret = gpio_pin_configure(gpio1, RX_EN_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
		if (ret < 0) {
			return ret;
		}
	}
/* 
	ret = gpio_pin_configure(gpio1, PDN_EN_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
	if (ret < 0) {
		return ret;
	}
*/
	return ret;

}

static enum benchmark_role device_role_select(void)
{
	char role;

	while (true) {
		printk("Choose device role - type r (receiver role) or b "
		       "(broadcaster role), or q to quit: ");
#if (CONFIG_ISO_TX_PA_MODE == 1)
	printk("Broadcaster role\n");
	init_pa_lna(1);
	return ROLE_BROADCASTER;
#endif


#if CONFIG_ISO_RX_LNA_MODE == 1
	printk("Receiver role\n");
	init_pa_lna(0);
	return ROLE_RECEIVER;
#endif
		role = tolower(console_getchar());

		printk("%c\n", role);

		if (role == 'r') {
			printk("Receiver role\n");
			return ROLE_RECEIVER;
		} else if (role == 'b') {
			printk("Broadcaster role\n");
			return ROLE_BROADCASTER;
		} else if (role == 'q') {
			printk("Quitting\n");
			return ROLE_QUIT;
		} else if (role == '\n') {
			continue;
		}

		printk("Invalid role: %c\n", role);
	}
}


void main(void)
{
	int err;
	enum benchmark_role role;

	LOG_INF("Starting Bluetooth Throughput example");

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_INF("Bluetooth init failed (err %d)", err);
		return;
	}

	err = console_init();
	if (err != 0) {
		LOG_INF("Console init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");


	while (true) {
		role = device_role_select();

		if (role == ROLE_RECEIVER) {
			err = test_run_receiver();
		} else if (role == ROLE_BROADCASTER) {
			err = test_run_broadcaster();
		} else {
			if (role != ROLE_QUIT) {
				LOG_INF("Invalid role %u", role);
				continue;
			} else {
				err = 0;
				break;
			}
		}

		LOG_INF("Test complete %d", err);
	}

	LOG_INF("Exiting");
}
