/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <stdio.h>
#include "rpc_app_nus.h"
#include "nrf_rpc_tr.h"
#include <logging/log.h>
#include <drivers/uart.h>

#define LOG_MODULE_NAME rpc_thread
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

extern struct k_sem sem_rpc_tx;

#define BUFFER_LENGTH 64

uint8_t buffer[BUFFER_LENGTH];

#ifdef CONFIG_RPC_REMOTE_API
static void bt_recv_cb(uint8_t *buffer, uint16_t length)
{
	LOG_INF("received ble data from netcore");
	LOG_HEXDUMP_INF(buffer, length, "data:");
}
#endif //CONFIG_RPC_REMOTE_API
static bool m_is_connected;
bool get_ble_connection_status(void)
{
	return m_is_connected;
}

void set_ble_connection_status(bool is_connected)
{
	m_is_connected = is_connected;
}

#ifdef CONFIG_RPC_SIMULATE_UART

#ifdef CONFIG_EXAMPLE_HS_UART
extern int my_uart_send(const uint8_t *buf, size_t len);
#endif
/*
* format: 0x55 CMD LEN Payload 0xAA (Checksum)
* 0x55 0x01 0x01 0x00 0xAA  //connected
* 0x55 0x01 0x01 0x01 0xAA  //disconnected
*/
/* Callback from transport layer that handles incoming. */
static void rpc_receive_handler(const uint8_t *packet, size_t len)
{
	
	LOG_INF("received data from net core by RPC");
	LOG_HEXDUMP_INF(packet, len, "packet:");

#ifdef CONFIG_EXAMPLE_HS_UART
	my_uart_send(packet, len);
#endif
	if(packet[0] == 0x55)
	{
		if (packet[1] == 0x01)
		{
			if (packet[2] == 0x01)
			{
				set_ble_connection_status(packet[3] == 0);
			}
		}		
	}
	if (!NRF_RPC_TR_AUTO_FREE_RX_BUF) {
		nrf_rpc_tr_free_rx_buf(packet);
	}
}
#endif //CONFIG_RPC_SIMULATE_UART

void rpc_example_init(void)
{
#ifdef CONFIG_RPC_REMOTE_API
	rpc_app_register_bt_recv_cb(bt_recv_cb);
#endif

#ifdef CONFIG_RPC_SIMULATE_UART
	int err = nrf_rpc_tr_init(rpc_receive_handler);
	if (err < 0) {
		LOG_ERR("rpc tr init error %d \n", err);
		return;
	}	
#endif
}

void rpc_tx_app(void)
{
	static uint8_t cnt;
	int err;
#ifdef CONFIG_RPC_REMOTE_API
		snprintf(buffer, 12, "HelloApp%d", cnt++);		
		buffer[11] = 0;		
		err = rpc_app_bt_nus_send(buffer, 12);
		if (err) {
			LOG_ERR("rpc_app_bt_nus_send failed: %d\n", err);			
		}
		else
		{
			LOG_HEXDUMP_INF(buffer, 12, "sent by app core:");
		}
#endif

#ifdef CONFIG_RPC_SIMULATE_UART
		snprintf(buffer, 12, "HelloApp%d", cnt++);		
		buffer[11] = 0;		
		err = nrf_rpc_tr_send(buffer, 12);
		if (err) {
			LOG_ERR("nrf_rpc_tr_send failed: %d\n", err);			
		}
		else
		{
			LOG_HEXDUMP_INF(buffer, 12, "Sent by app core:");
		}
#endif
}

void rpc_thread(void)
{
	LOG_INF("**RPC usage example\n");
    rpc_example_init();

	while (1) {        
		k_sem_take(&sem_rpc_tx, K_FOREVER);	
		LOG_INF("RPC thread\n");	
		rpc_tx_app();
	}
}

K_THREAD_DEFINE(rpc_thread_id, 1024, rpc_thread, NULL, NULL,
		NULL, 6, 0, 0);