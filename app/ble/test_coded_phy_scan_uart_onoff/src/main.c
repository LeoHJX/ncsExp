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
#include <stdio.h>
#include <string.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/scan.h>

#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS DT_GPIO_FLAGS(LED0_NODE, gpios)

//static bt_addr_t addr =   {0x28, 0x80, 0x53, 0x67, 0xED, 0xE3};

#define SCAN_OUT_TO_CONSOLE 0

#if !SCAN_OUT_TO_CONSOLE
#define SCAN_OUT_TO_UART_ONOFF 1
#endif

static const uint8_t dev_name[] = "Test beacon";

static uint8_t count = 0;
#if SCAN_OUT_TO_UART_ONOFF
#define UART_DEV_NAME "UART_0"

static const struct device *uart_dev;

static inline void write_uart_string(const char *str)
{
	/* Send characters until, but not including, null */
	for (size_t i = 0; str[i]; i++) {
		uart_poll_out(uart_dev, str[i]);
	}
}

static void uart_rx_handler(uint8_t character) /* loop back here.  */
{
	uart_poll_out(uart_dev, character);
}

static void isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	uint8_t character;

	uart_irq_update(dev);

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	/*
	 * Check that we are not sending data (buffer must be preserved then),
	 * and that a new character is available before handling each character
	 */
	while (uart_fifo_read(dev, &character, 1)) {
		uart_rx_handler(character);
	}
}

static int at_uart_init(char *uart_dev_name)
{
	int err;
	uint8_t dummy;

	uart_dev = device_get_binding(uart_dev_name);
	if (uart_dev == NULL) {
		printk("Cannot bind %s\n", uart_dev_name);
		return -EINVAL;
	}

	uint32_t start_time = k_uptime_get_32();

	/* Wait for the UART line to become valid */
	do {
		err = uart_err_check(uart_dev);
		if (err) {
			if (k_uptime_get_32() - start_time >
			    500 /* uart init timeout */) {
				printk("UART check failed: %d. "
				       "UART initialization timed out.",
				       err);
				return -EIO;
			}

			printk("UART check failed: %d. "
			       "Dropping buffer and retrying.",
			       err);

			while (uart_fifo_read(uart_dev, &dummy, 1)) {
				/* Do nothing with the data */
			}
			k_sleep(K_MSEC(10));
		}
	} while (err);

	uart_irq_callback_set(uart_dev, isr);

	device_set_power_state(uart_dev, DEVICE_PM_ACTIVE_STATE, NULL, NULL);

	return err;
}

static int uart_de_init(void)
{
	int err;
	//k_sleep(K_MSEC(100)); /* wait for sometime, make sure data been send out */
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	k_sleep(K_MSEC(2));
	err = device_set_power_state(uart_dev, DEVICE_PM_OFF_STATE, NULL, NULL);
	if (err) {
		printk("Can't power off uart: %d", err);
	}

	return err;
}

static int uart_init(void)
{
	int err;
	/* Initialize the UART module */
	err = at_uart_init(UART_DEV_NAME);
	if (err) {
		printk("UART could not be initialized: %d", err);
		return -EFAULT;
	}

	uart_irq_rx_enable(uart_dev);

	return err;
}

#endif
static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
#if SCAN_OUT_TO_CONSOLE

	int8_t temp[255];
	uint8_t data_offset = 0;
	printk("Scanned Test Beacon count = %d\n", count++);
	/* possible to add futher filtering here.   */
	printk("adv data len: %d\n", device_info->adv_data->len);
	printk("recv adrs type: %d\n", device_info->recv_info->addr->type);
	printk("recv adrs:%02X%02X%02X%02X%02X%02X\n",
	       device_info->recv_info->addr->a.val[0],
	       device_info->recv_info->addr->a.val[1],
	       device_info->recv_info->addr->a.val[2],
	       device_info->recv_info->addr->a.val[3],
	       device_info->recv_info->addr->a.val[4],
	       device_info->recv_info->addr->a.val[5]);
	printk("RSSI: %d\n", device_info->recv_info->rssi);
	printk("recv data0 type : %02x\n", device_info->adv_data->data[1]);
	printk("recv data0 len :  %d\n", device_info->adv_data->data[0]);
	memset(temp, 0, 255);
	memcpy(temp, &(device_info->adv_data->data[2]),
	       device_info->adv_data->data[0] - 1);
	printk("recv data0 data : %s\n", temp);

	data_offset = device_info->adv_data->data[0] +
		      1; /* +2: offset the lengh byte */

	printk("recv data1 type : %02x\n",
	       device_info->adv_data->data[data_offset + 1]);
	printk("recv data1 len :  %d\n",
	       device_info->adv_data->data[data_offset]);
	printk("recv data1 id :  %02X%02X\n",
	       device_info->adv_data->data[data_offset + 3],
	       device_info->adv_data->data[data_offset + 2]);
	memset(temp, 0, 255);
	memcpy(temp, &(device_info->adv_data->data[data_offset + 4]),
	       device_info->adv_data->data[data_offset] -
		       3); /* 3 bytes including length, and manufacture ID 2 bytes */
	printk("recv data1 data : %s\n", temp);

#else

#if SCAN_OUT_TO_UART_ONOFF
	int8_t temp[150];
        int8_t temp1[100];
	uint8_t data_offset = 0;
	printk("uart enable and ouput\r\n");
	uart_init();
        
	sprintf(temp, "Scanned Test Beacon count = %d\r\n", count++);
	write_uart_string(temp);
        
	/* possible to add futher filtering here.   */
	sprintf(temp, "adv data len: %d\r\n", device_info->adv_data->len);
	write_uart_string(temp);
	sprintf(temp, "recv adrs type: %d\r\n",
		device_info->recv_info->addr->type);
	write_uart_string(temp);
	sprintf(temp, "recv adrs:%02X%02X%02X%02X%02X%02X\r\n",
		device_info->recv_info->addr->a.val[0],
		device_info->recv_info->addr->a.val[1],
		device_info->recv_info->addr->a.val[2],
		device_info->recv_info->addr->a.val[3],
		device_info->recv_info->addr->a.val[4],
		device_info->recv_info->addr->a.val[5]);
	write_uart_string(temp);
	sprintf(temp, "RSSI: %d\r\n", device_info->recv_info->rssi);
	write_uart_string(temp);
	sprintf(temp, "recv data0 type : %02x\r\n",
		device_info->adv_data->data[1]);
	write_uart_string(temp);
	sprintf(temp, "recv data0 len :  %d\r\n", device_info->adv_data->data[0]);
	write_uart_string(temp);
       #if 1
	memset(temp1, 0, 100);
	memcpy(temp1, &(device_info->adv_data->data[2]),
	       device_info->adv_data->data[0] - 1);
	sprintf(temp, "recv data0 data : %s\r\n", temp1);
	write_uart_string(temp);
	data_offset = device_info->adv_data->data[0] +
		      1; /* +2: offset the lengh byte */

	sprintf(temp, "recv data1 type : %02x\r\n",
		device_info->adv_data->data[data_offset + 1]);
	write_uart_string(temp);
	sprintf(temp, "recv data1 len :  %d\r\n",
		device_info->adv_data->data[data_offset]);
	write_uart_string(temp);
	sprintf(temp, "recv data1 id :  %02X%02X\r\n",
		device_info->adv_data->data[data_offset + 3],
		device_info->adv_data->data[data_offset + 2]);
	write_uart_string(temp);
	memset(temp1, 0, 100);
	memcpy(temp1, &(device_info->adv_data->data[data_offset + 4]),
	       device_info->adv_data->data[data_offset] -
		       3); /* 3 bytes including length, and manufacture ID 2 bytes */
	sprintf(temp, "recv data1 data : %s\r\n", temp1);
	write_uart_string(temp);
        #endif
	uart_de_init();

#endif

#endif

	/* device_info->conn_param;
    
    filter_match->addr;
    filter_match->appearance;
    filter_match->manufacturer_data;
    filter_match->name;
    filter_match->short_name;
    filter_match->uuid;
    connectable;  
    */
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

void main(void)
{
	int err;

	const struct device *dev;

	bool led_is_on = true;

	struct bt_le_scan_param scan_param = {
		.type = BT_HCI_LE_SCAN_PASSIVE,
		.options = BT_LE_SCAN_OPT_CODED,
		.interval = BT_GAP_SCAN_SLOW_INTERVAL_1,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};

	struct bt_scan_init_param scan_init = { .connect_if_match = 0,
						.scan_param = &scan_param,
						.conn_param = NULL };

	uart_init();

	printk("Starting Scanner Demo\n");
	write_uart_string("Starting Scanner Demo\n"); /* out to UART */
	dev = device_get_binding(LED0);
	if (dev == NULL) {
		return;
	}

	err = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (err < 0) {
		return;
	}

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_scan_init(&scan_init);

	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, dev_name);
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
	}
	uart_de_init(); /*  off uart to save power and only enable if data avaiable out */
	bt_scan_start(BT_SCAN_TYPE_SCAN_PASSIVE);

	do {
		gpio_pin_set(dev, PIN, (int)led_is_on);
		led_is_on = !led_is_on;

		k_sleep(K_MSEC(500));

	} while (1);
}