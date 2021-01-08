/*

* Copyright (c) 2012-2014 Wind River Systems, Inc.

*

* SPDX-License-Identifier: Apache-2.0

*/

#include <zephyr.h>
#include <sys/printk.h>


#include <drivers/spi.h>

/* including 

	const struct device	*gpio_dev;
	uint32_t		delay;
	gpio_pin_t		gpio_pin;
	gpio_dt_flags_t		gpio_dt_flags;
*/

struct spi_cs_control cs_ctrl;


static struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 4000000,
	.slave = 0,
};

const struct device * spi_dev;

static void spi_init(void)
{

	cs_ctrl.delay = 20U;
	cs_ctrl.gpio_pin = 7;  /* px.07; GPIO_0 -> p0.07 */
	cs_ctrl.gpio_dt_flags = GPIO_ACTIVE_LOW;
	cs_ctrl.gpio_dev = device_get_binding("GPIO_0");

	if (!cs_ctrl.gpio_dev) {
		printk("Unable to get GPIO SPI CS device");
		return ;
	}
	spi_cfg.cs = &cs_ctrl;
	const char* const spiName = "SPI_1";
	spi_dev = device_get_binding(spiName);

	if (spi_dev == NULL) {
		printk("Could not get %s device\n", spiName);
		return;
	}
}

void spi_test_send(void)
{
	int err;
	static uint8_t tx_buffer[1];
	static uint8_t rx_buffer[1];

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = sizeof(rx_buffer),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		printk("TX sent: %x\n", tx_buffer[0]);
		printk("RX recv: %x\n", rx_buffer[0]);
		tx_buffer[0]++;
	}	
}

void main(void)
{
	printk("SPIM Example\n");
	spi_init();

	while (1) {
		spi_test_send();
		k_msleep(1000);
	}
}
