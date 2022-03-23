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
	//cs_ctrl.gpio_dt_flags = 1;  // only avaiable in later NCS releases, please enable this line when compile with later ncs versions. 
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
	const char *const spiName = "SPI_1";
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


void spi_dummy_writes(void)  /* read electronic signature  */
{
	int err;
	uint8_t tx_buffer[32];
	uint8_t rx_buffer[32];
    uint8_t tx_return[32];

	const struct spi_buf tx_buf = { .buf = tx_buffer,
					.len = 2 };
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
    tx_buffer[0] = 0x0d;
    tx_buffer[1] = 0x0a;
	err = spi_transceive(spi_dev, &spi_cfg, &tx, NULL);
	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		printk("ES: Send: %x %x\n", tx_buffer[0], tx_buffer[1] );
		printk("RX recv: %02x %02x %02x %02x\n", rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]  /* */);
		
	}
}
unsigned long spi_chk[50][3];
unsigned int spi_chk_cnt = 0;

static int8_t spi_rw(uint8_t* pu8Mosi, uint8_t* pu8Miso, uint16_t u16Sz)
{
//   HAL_StatusTypeDef status;  
int8_t status;
    int err;

/*--- RX ----------------------------------------------------------------------------*/
//const   struct spi_buf rx_buf[] = {
const   struct spi_buf spi_rx_buf = {
                                  .buf = pu8Miso,
                                  .len = u16Sz,
                                  };

const    struct spi_buf_set rx = {
//		.buffers = spi_rx_buf,
		.buffers = &spi_rx_buf,
//		.count = sizeof(spi_rx_buf) / sizeof(struct spi_rx_buf)
                .count = 1,
                };

 /*--- TX ----------------------------------------------------------------------------*/
const  struct spi_buf spi_tx_buf = {
            .buf = pu8Mosi,
            .len = u16Sz
//            .len = u16Sz/sizeof(uint8_t)
    };
const struct  spi_buf_set tx = {
//            .buffers = spi_tx_buf,           
             .buffers = &spi_tx_buf,
//            .count = sizeof(spi_tx_buf)/sizeof(struct spi_tx_buf)
            .count = 1,
    };
/*----------------------------------------------------------------------------------*/
printk("\r\nCID: 0x%x ", (uint32_t)(k_current_get ()));
if( (pu8Mosi == NULL) && (pu8Miso == NULL) )
{
    printk("%s: SPI_null data ptrs: Len (%d)\n",__FUNCTION__,u16Sz);
    return -1;
}

printk("WF_spi: T%08lX-R%08lX-%u",pu8Mosi,pu8Miso,u16Sz);
if (spi_chk_cnt >= 50)
  spi_chk_cnt = 0;

spi_chk[spi_chk_cnt][0] = pu8Mosi;
spi_chk[spi_chk_cnt][1] = pu8Miso;
spi_chk[spi_chk_cnt][2] = u16Sz;

uint8_t tx_junk[8] = {0xff,0xff };

    /* Start SPI transaction - polling method */
    /* Transmit/Recieve */
    if (pu8Mosi == NULL)                                              /* Read Data */
	{
//   err = spi_read(winc1500.spiDev, &winc1500.spi_conf, &rx);
//		status = HAL_SPI_TransmitReceive(&hspiWifi,spiDummyBuf,pu8Miso,u16Sz,1000);


//err = spi_transceive(winc1500.spiDev,&winc1500.spi_conf, (uint8_t* )(tx_junk), &rx);
err = spi_transceive(spi_dev, &spi_cfg, NULL, &rx);

    }
    else if(pu8Miso == NULL)                                          /* Write Data */
    {
       uint32_t datalen = 0;
       uint8_t* dataptr = tx.buffers->buf;
//    err = spi_write(winc1500.spiDev, &winc1500.spi_conf, &tx);
//        status = HAL_SPI_TransmitReceive(&hspiWifi,pu8Mosi,spiDummyBuf,u16Sz,1000);
 //       memset(spiDummyBuf,0, u16Sz);
 	err = spi_transceive(spi_dev, &spi_cfg, &tx, NULL);
    while(datalen < tx.buffers->len)
    {
        printk("%02x ", dataptr[datalen++]);
    }
    
   }
    else
    {     
    err = spi_transceive(spi_dev, &spi_cfg, &tx, &rx); 
//        status = HAL_SPI_TransmitReceive(&hspiWifi,pu8Mosi,pu8Miso,u16Sz,1000);
    } 
 
    

    /* Handle Transmit/Recieve error */
    if (err != 0)
    {
        printk("%s: HAL_SPI_TransmitReceive failed. error (%d)\n",__FUNCTION__,err);
        spi_chk_cnt++;
       // k_sleep(K_MSEC(10));
        return status;
    }
    
 // 	spi_select_slave(false);

        spi_chk_cnt++;
       // k_sleep(K_MSEC(10));
	return 0;
}

void spi_dummy_write_2(void)
{
    uint8_t wb[32];
    uint8_t readcnt = 20;
    
    while(readcnt-->0)
    {
         spi_rw(NULL, wb, 1);  
    }
    spi_rw(NULL, wb, 3);  
    wb[0] = 0xc7;
    wb[1] = 0x0d;
    wb[2] = 0xda;
    wb[3] = 0x38;
    wb[4] = 0x00;
    wb[5] = 0x00;
    wb[6] = 0x02;          
    spi_rw(wb, NULL, 7);  
    spi_rw(NULL, wb, 1);  
    spi_rw(NULL, wb, 1);
    wb[0] = 0xf3;
    spi_rw(wb, NULL, 1); 
    wb[0] = 0xd;
    wb[1] = 0x0a;
    spi_rw(wb, NULL, 2);
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
        spi_dummy_writes();
		spi_read_rdid();  //9f
        spi_dummy_write_2();
        //k_sleep(K_MSEC(100));
        spi_read_res();  //ab
        //k_sleep(K_MSEC(100));
        spi_read_rems(); //90
		k_sleep(K_MSEC(500));
	}
}
