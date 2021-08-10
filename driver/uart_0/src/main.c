/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <stdlib.h>
#include <sys/printk.h>
#include <stdio.h>
#include <ctype.h>
#include <drivers/uart.h>

#define UART_DEV_NAME "UART_0"

/*  various testing options */

#define UART_USE_RX_INTERRUPT 1

#define UART_SIMPLE_EN_DIS_POWER_SAVING 0 /* this option doesn't work; can't resume UART after LP */

#if !UART_SIMPLE_EN_DIS_POWER_SAVING
/* nrf52840 have to use Zephyr power manager to gets to LP stage, or add a workaround, but doesn't resume to normal operations. */
/* this options require add "CONFIG_DEVICE_POWER_MANAGEMENT=y" to prj.conf */
/* verified with 52833, 52840; more to add */
//#define UART_LP_USE_Z_PWR_MANAGER 1
#endif

/*   testing options end */

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
#if UART_USE_RX_INTERRUPT
	uart_irq_callback_set(uart_dev, isr);
#endif

#if UART_LP_USE_Z_PWR_MANAGER
	device_set_power_state(uart_dev, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
#endif
	return err;
}

#if UART_LP_USE_Z_PWR_MANAGER

static int uart_de_init(void)
{
	int err;
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	k_sleep(K_MSEC(100));
	err = device_set_power_state(uart_dev, DEVICE_PM_OFF_STATE, NULL, NULL);
	if (err) {
		printk("Can't power off uart: %d", err);
	}

	return err;
}

#endif

static int uart_init(void)
{
	int err;
	/* Initialize the UART module */
	err = at_uart_init(UART_DEV_NAME);
	if (err) {
		printk("UART could not be initialized: %d", err);
		return -EFAULT;
	}
#if UART_USE_RX_INTERRUPT
	uart_irq_rx_enable(uart_dev);
#endif
	return err;
}

#if UART_SIMPLE_EN_DIS_POWER_SAVING
/* simple uart enable via ENABLE registor */
void enable_uart(void)
{
	NRF_UARTE0->ENABLE = 0x8;
	//NRF_UARTE1->ENABLE = 0x8;
}
/* simple uart disable via ENABLE registors */
void disable_uart(void)
{
	NRF_UARTE0->ENABLE = 0;
	//NRF_UARTE1->ENABLE = 0;
#if 0  /* enable this for nRF52840 work around, Note: doesn't return normal again */
	// Workaround current consumption issue by power cycling the UART peripherals
	*(volatile uint32_t *)0x40002FFC = 0; // Power down UARTE0
	*(volatile uint32_t *)0x40002FFC; //
	*(volatile uint32_t *)0x40002FFC =
		1; // Power on UARTE0 so it is ready for next time
#if 1  /* for UARTE1*/
	*(volatile uint32_t *)0x40028FFC = 0; // Power down UARTE1
	*(volatile uint32_t *)0x40028FFC; //
	*(volatile uint32_t *)0x40028FFC =
		1; // Power on UARTE1 so it is ready for next time
#endif
#endif
}
#endif
void main(void)
{
	uint8_t buf[100];
	memset(buf, 0, 100);
	uart_init();
	printk("Hello World! %s\n",
	       CONFIG_BOARD); /* should out from UART0 if enabled */
	sprintf(buf, "Hello World! from %s %s\n\r", UART_DEV_NAME,
		CONFIG_BOARD);
	write_uart_string(buf);
	write_uart_string("Now Starting Looping back\r\n");

#if UART_SIMPLE_EN_DIS_POWER_SAVING
	k_sleep(K_MSEC(4000));
	disable_uart();
	k_sleep(K_MSEC(4000)); /* should be low power @ few uA */
	enable_uart();
	printk("Hello World! %s, after disable->Enable!!\n",
	       CONFIG_BOARD); /* should out from UART0 if enabled */
	sprintf(buf, "Hello World! from %s %s, after disable->Enable!!\n\r",
		UART_DEV_NAME, CONFIG_BOARD);
	write_uart_string(buf);
#else

#ifdef UART_LP_USE_Z_PWR_MANAGER
	k_sleep(K_MSEC(4000));
	uart_de_init();
	k_sleep(K_MSEC(4000)); /* should be low power @ few uA */
	uart_init();
	printk("Hello World! %s, after disable->Enable!!\n",
	       CONFIG_BOARD); /* should out from UART0 if enabled */
	sprintf(buf, "Hello World! from %s %s, after disable->Enable!!\n\r",
		UART_DEV_NAME, CONFIG_BOARD);
	write_uart_string(buf);

#endif

#endif
	/*  looping with non-interrupt */
#if !UART_USE_RX_INTERRUPT /* enable echo with polling mode */
	while (1) {
		uint8_t temp;
		while (uart_fifo_read(uart_dev, &temp, 1)) {
			uart_poll_out(uart_dev, temp);
		}
	}
#endif
}
