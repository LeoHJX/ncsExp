/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <drivers/uart.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/printk.h>
#include <zephyr.h>

#define DRV_USE_ISR 1

#define UART_DEV_NAME "UART_0"

static const struct device *uart_dev;

static inline void write_uart_string(const char *str)
{
	/* Send characters until, but not including, null */
	for (size_t i = 0; str[i]; i++) {
		uart_poll_out(uart_dev, str[i]);
	}
}

#if DRV_USE_ISR
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
#endif
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
#if DRV_USE_ISR
	uart_irq_callback_set(uart_dev, isr);
#endif
#if 1
	/* Power on UART module */
	device_set_power_state(uart_dev, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
#endif
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
#if DRV_USE_ISR
	uart_irq_rx_enable(uart_dev);
#endif
	return err;
}

void uart_lp_cycle(void)
{
	// Workaround current consumption issue by power cycling the UART peripherals
	*(volatile uint32_t *)0x40002FFC = 0; // Power down UARTE0
	*(volatile uint32_t *)0x40002FFC; //
	*(volatile uint32_t *)0x40002FFC =
		1; // Power on UARTE0 so it is ready for next time
#if 1
	*(volatile uint32_t *)0x40028FFC = 0; // Power down UARTE1
	*(volatile uint32_t *)0x40028FFC; //
	*(volatile uint32_t *)0x40028FFC =
		1; // Power on UARTE1 so it is ready for next time
#endif
}

void uart_lp_pwr_off(void)
{
	*(volatile uint32_t *)0x40002FFC = 0; // Power down UARTE0
	*(volatile uint32_t *)0x40028FFC = 0; // Power down UARTE1
	*(volatile uint32_t *)0x40002FFC;
	*(volatile uint32_t *)0x40028FFC;
	//NRF_UARTE0->ENABLE = 0x00;
	//NRF_UARTE1->ENABLE = 0x00;
	/*
        NRF_UARTE0->TASKS_STOPRX = 0;
        NRF_UARTE0->TASKS_STOPTX = 0;
        NRF_UARTE1->TASKS_STOPRX = 0;
        NRF_UARTE1->TASKS_STOPTX = 0;
        */
}

void uart_lp_pwr_on(void)
{
	*(volatile uint32_t *)0x40002FFC;
	*(volatile uint32_t *)0x40028FFC;
	*(volatile uint32_t *)0x40002FFC = 1; // Power on UARTE0
	*(volatile uint32_t *)0x40028FFC = 1; // Power on UARTE1

	//NRF_UARTE0->EVENTS_TXSTARTED;
	//NRF_UARTE0->EVENTS_RXSTARTED;
	//NRF_UARTE0->ENABLE = 0x08;
	//NRF_UARTE1->ENABLE = 0x08;
	//NRF_UARTE0->TASKS_STARTRX = 1;
	//NRF_UARTE1->TASKS_STARTRX = 1;

	/*
      NRF_UARTE0->TASKS_STOPRX = 1;
      NRF_UARTE0->TASKS_STOPTX = 1;
      NRF_UARTE1->TASKS_STOPRX = 1;
      NRF_UARTE1->TASKS_STOPTX = 1;
 */
}

void uart_reg_dbg_out(void)
{
	uint32_t adrs = 0x40002000;

	do {
		printk("adrs: %04X = val: %X\n", adrs & 0xffff,
		       *((volatile uint32_t *)adrs));
		adrs = adrs + 4;
	} while (adrs <= 0x4000256C);
}

void main(void)
{
	int err;
	uint8_t buf[100];
	struct uart_config uart_cfg;

	///uart_lp_pwr_on();
	memset(buf, 0, 100);
	k_sleep(K_MSEC(1000));
	//uart_reg_dbg_out();
	uart_init();
	printk("Hello World! %s\n",
	       CONFIG_BOARD); /* should out from UART0 if enabled */
	sprintf(buf, "Hello World! from %s %s\n\r", UART_DEV_NAME,
		CONFIG_BOARD);
	write_uart_string(buf);
	k_sleep(K_MSEC(2000));
	printk("disable UART now\n");
#if 0
        //uart_config_get(uart_dev, &uart_cfg);
        uart_rx_disable(uart_dev);
	k_sleep(K_MSEC(100));
	err = device_set_power_state(uart_dev, DEVICE_PM_OFF_STATE,
				NULL, NULL);
	if (err) {
		printk("Can't power off uart: %d", err);
	}

#endif
	//uart_reg_dbg_out();

	NRF_UARTE0->ENABLE = 0x00;
	NRF_UARTE1->ENABLE = 0x00;
	uart_lp_pwr_off();
	k_sleep(K_MSEC(2000));
	uart_lp_pwr_on();
	k_sleep(K_MSEC(100));

	NRF_UARTE0->ENABLE = 0x08;
	NRF_UARTE1->ENABLE = 0x08;
	//uart_init();
#if 0
      *(volatile uint32_t *)0x40002508 = 5;
      *(volatile uint32_t *)0x4002850C = 6;
      *(volatile uint32_t *)0x40002510 = 7;
      *(volatile uint32_t *)0x40028514 = 8;
      //NRF_UARTE0->ENABLE = 0x08;
     // NRF_UARTE1->ENABLE = 0x08;
      *(volatile uint32_t *)0x4002850C = 6;
      *(volatile uint32_t *)0x40028514 = 8;
#endif
	/*
        err = uart_configure(uart_dev, &uart_cfg);
	if (err != 0) {
            printk("uart_configure: %d", err);
	}
        */
	k_sleep(K_MSEC(100));
	//uart_reg_dbg_out();

	printk("Hello World! %s\n",
	       "Start UART again!!"); /* should out from UART0 if enabled */
	sprintf(buf, "Hello World! from %s %s\n\r", UART_DEV_NAME,
		"Start UART again!!");
	write_uart_string(buf);
	/*  looping with non-interrupt */

#if 0 /* enable echo with polling mode */
        while(1){
            uint8_t temp;
            while (uart_fifo_read(uart_dev, &temp, 1)){
                uart_poll_out(uart_dev, temp);
            }
        }
#endif
}