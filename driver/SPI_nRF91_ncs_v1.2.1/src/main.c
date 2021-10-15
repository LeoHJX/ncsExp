/*

* Copyright (c) 2012-2014 Wind River Systems, Inc.

*

* SPDX-License-Identifier: Apache-2.0

*/

#define TEST_SPI_CS_EN 1
#define SPI_CS_CFG_FROM_DTS 0

#if SPI_CS_CFG_FROM_DTS
#define DT_DRV_COMPAT jedec_spi_nor
#endif

#include <zephyr.h>
#include <sys/printk.h>

#include <drivers/spi.h>

/* including 

	const struct device	*gpio_dev;
	uint32_t		delay;
	gpio_pin_t		gpio_pin;
	gpio_dt_flags_t		gpio_dt_flags;
*/

#if TEST_SPI_CS_EN
struct spi_cs_control cs_ctrl; /*   */
#endif

static struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) /* | SPI_TRANSFER_MSB | SPI_MODE_CPOL |
		     SPI_MODE_CPHA*/,
	.frequency = 1000000,
	.slave = 0,
};

const struct device *spi_dev;

static void spi_init(void)
{
#if TEST_SPI_CS_EN
#if !SPI_CS_CFG_FROM_DTS /* not from DTS */
	/* A quick dirty way to add CS to SPI Master to control slave CS  */
	cs_ctrl.delay = 0U; /* us time setup CS before and after SPI activities   */
	cs_ctrl.gpio_pin = 25; /* px.07; GPIO_0 -> p0.25 */
	cs_ctrl.gpio_dt_flags = 1;  // only avaiable in later NCS releases, please enable this line when compile with later ncs versions. 
	cs_ctrl.gpio_dev = device_get_binding("GPIO_0");

	if (!cs_ctrl.gpio_dev) {
		printk("Unable to get GPIO SPI CS device");
		return;
	}
#else

/* require dts overlay includes the cs pin details, propery device driver setup.*/
#if DT_INST_SPI_DEV_HAS_CS_GPIOS(0)
	cs_ctrl.delay = 0U;
	cs_ctrl.gpio_pin =
		DT_INST_SPI_DEV_CS_GPIOS_PIN(0); /* */
	cs_ctrl.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0);
	cs_ctrl.gpio_dev =
		device_get_binding(DT_INST_SPI_DEV_CS_GPIOS_LABEL(0));

	if (!cs_ctrl.gpio_dev) {
		printk("Unable to get GPIO SPI CS device");
		return;
	}

#endif
#endif
	spi_cfg.cs = &cs_ctrl;
#endif
	const char *const spiName = "SPI_3";
	spi_dev = device_get_binding(spiName);

	if (spi_dev == NULL) {
		printk("Could not get %s device\n", spiName);
		return;
	}
}

void spi_read_rdid(void)  /* read device id */
{
	int err;
	uint8_t tx_buffer[32];
	uint8_t rx_buffer[32];
    uint8_t tx_return[32];

	const struct spi_buf tx_buf = { .buf = tx_buffer,
					.len = 1 };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	struct spi_buf rx_buf[2] = {
        {
            .buf = tx_return,
            .len = 1
        },
        {
            .buf = rx_buffer,
            .len = 3   /* 3 */
        }
	};
	const struct spi_buf_set rx = { .buffers = (struct spi_buf*)(&rx_buf), .count = 2 };
    tx_buffer[0] = 0x9f;
	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		printk("ID, send: %x\n", tx_buffer[0]);
		printk("RX recv: %02x %02x %02x\n", rx_buffer[0], rx_buffer[1], rx_buffer[2]  /* */);
		
	}
}


void spi_read_res(void)  /* read electronic signature  */
{
	int err;
	uint8_t tx_buffer[32];
	uint8_t rx_buffer[32];
    uint8_t tx_return[32];

	const struct spi_buf tx_buf = { .buf = tx_buffer,
					.len = 1 };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	struct spi_buf rx_buf[2] = {
        {
            .buf = tx_return,
            .len = 1
        },
        {
            .buf = rx_buffer,
            .len = 4
        }
	};
	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 2 };
    tx_buffer[0] = 0xAB;
	err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		printk("ES: Send: %x\n", tx_buffer[0]);
		printk("RX recv: %02x %02x %02x %02x\n", rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]  /* */);
		
	}
}


void spi_read_rems(void)  /* read electronic manufacture & device id  */
{
	int err;
	uint8_t tx_buffer[32];
	uint8_t rx_buffer[32];
    uint8_t tx_return[32];

	const struct spi_buf tx_buf = { .buf = tx_buffer,
					.len = 1 };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	struct spi_buf rx_buf[2] = {
        {
            .buf = tx_return,
            .len = 0 /* 1 */
        },
        {
            .buf = rx_buffer,
            .len = 5  /* 5 */     /*   use a wrong number on purpose */
        }
	};
	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 2 };
    tx_buffer[0] = 0x90;
	err = spi_transceive(spi_dev, &spi_cfg, NULL, &rx);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		printk("REMS: Send: %x\n", tx_buffer[0]);
		printk("RX recv: %02x %02x %02x %02x %02x\n", rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4] /* */);
		
	}
}

void main(void)
{
	printk("SPIM Example\n");
	spi_init();
    /* Validate bus and CS is ready */
  //  if (!spi_is_ready(&spi_cfg)) {
  //      return -ENODEV;
  //  }
    
	while (1) {
        printk("###################################################\n");
		spi_read_rdid();  //9f
        k_sleep(K_MSEC(100));
        spi_read_res();  //ab
        k_sleep(K_MSEC(100));
    	spi_read_rems(); //90
		k_sleep(K_MSEC(500));
	}
}
