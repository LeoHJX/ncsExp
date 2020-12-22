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

#define UART_DEV_NAME "UART_1"

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
					"UART initialization timed out.", err); 
				return -EIO;
			}

			printk("UART check failed: %d. "
				"Dropping buffer and retrying.", err); 

			while (uart_fifo_read(uart_dev, &dummy, 1)) {
				/* Do nothing with the data */
			}
			k_sleep(K_MSEC(10));
		}
	} while (err);

	uart_irq_callback_set(uart_dev, isr);
	return err;
}


static int uart1_init(void)
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
void main(void)
{ 
        uint8_t buf[100];
        memset(buf, 0, 100);
        uart1_init();
	printk("Hello World! %s\n", CONFIG_BOARD);  /* should out from UART0 if enabled */
        sprintf(buf, "Hello World! from %s %s\n\r", UART_DEV_NAME, CONFIG_BOARD);
        write_uart_string(buf);
        write_uart_string("Now Starting Looping back\r\n");
        /*  looping with non-interrupt */
#if 0  /* enable echo with polling mode */
        while(1){
            uint8_t temp;
            while (uart_fifo_read(uart_dev, &temp, 1)){
                uart_poll_out(uart_dev, temp);
            }
        }
#endif
}
