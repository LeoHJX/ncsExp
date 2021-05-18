/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <logging/log.h>
#include <nrfx_spim.h>
#include <nrfx_rtc.h>

#define LOG_MODULE_NAME raw_nrfx_thread
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define SPI_SS_PIN 44
#define SPI_MOSI_PIN 0x2d
#define SPI_MISO_PIN 0x2e
#define SPI_SCK_PIN 0x2f

static const nrfx_spim_t spi = NRFX_SPIM_INSTANCE(3);
extern struct k_sem sem_raw_nrfx_txrx;
K_SEM_DEFINE(sem_raw_spi_sync, 0, 1);
#define TEST_STRING "Nordic"
static uint8_t       m_tx_buf[] = TEST_STRING;           /**< TX buffer. */
static uint8_t       m_rx_buf[sizeof(TEST_STRING) + 1];    /**< RX buffer. */
static const uint8_t m_length = sizeof(m_tx_buf);        /**< Transfer length. */

static const nrfx_rtc_t rtc = NRFX_RTC_INSTANCE(0);
#define LED1 29
#define COMPARE_COUNTERTIME  (5UL)   

/**
 * @brief SPI user event handler.
 * @param event
 */
void spi_event_handler(nrfx_spim_evt_t const * p_event,
                       void *                    p_context)
{
    k_sem_give(&sem_raw_spi_sync);	
    LOG_INF("Transfer completed.");
    if (m_rx_buf[0] != 0)
    {        
        LOG_HEXDUMP_INF(m_rx_buf, strlen((const char *)m_rx_buf), "Received: ");
    }
}

static int spi_data_exchange(void)
{

	// Reset rx buffer
	memset(m_rx_buf, 0, m_length);

	nrfx_spim_xfer_desc_t const spim_xfer_desc =
	{
		.p_tx_buffer = m_tx_buf,
		.tx_length   = m_length,
		.p_rx_buffer = m_rx_buf,
		.rx_length   = m_length,
	};
	int ret = nrfx_spim_xfer(&spi, &spim_xfer_desc, 0);	
	if (ret != NRFX_SUCCESS)
	{
		LOG_ERR("raw spi xfer err %d", ret);
		return ret;
	}

	k_sem_take(&sem_raw_spi_sync, K_FOREVER);

	return 0;

}

static void raw_spi_init(void)
{
	nrfx_err_t rc;
	
	//use Zephyr's method to setup ISR
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SPIM3), 1,
		    nrfx_isr, nrfx_spim_3_irq_handler, 0);

    nrfx_spim_config_t spi_config = 
	{
		.sck_pin        = SPI_SCK_PIN,                \
		.mosi_pin       = SPI_MOSI_PIN,                \
		.miso_pin       = SPI_MISO_PIN,                \
		.ss_pin         = SPI_SS_PIN,                \
		.ss_active_high = false,                                 \
		.irq_priority   = NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY, \
		.orc            = 0xFF,                                  \
		.frequency      = NRF_SPIM_FREQ_4M,                      \
		.mode           = NRF_SPIM_MODE_0,                       \
		.bit_order      = NRF_SPIM_BIT_ORDER_MSB_FIRST,          \		
	};

    rc = nrfx_spim_init(&spi, &spi_config, spi_event_handler, NULL);
	if (rc != NRFX_SUCCESS)
	{
		LOG_ERR("raw spi init err %d", rc);
	}
	
}

static void rtc_handler(nrfx_rtc_int_type_t int_type)
{
    if (int_type == NRFX_RTC_INT_COMPARE0)
    {
		LOG_INF("raw RTC cc0 evt");
        nrf_gpio_pin_toggle(LED1);
    }
}

static void raw_rtc_init(void)
{
	nrfx_err_t rc;
	
	LOG_INF("*** Raw RTC usage example ***");
	//use Zephyr's method to setup ISR
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_RTC0), 1,
		    nrfx_isr, nrfx_rtc_0_irq_handler, 0);

    //Initialize RTC instance
    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler = 4095;
    rc = nrfx_rtc_init(&rtc, &config, rtc_handler);
	if (rc != NRFX_SUCCESS)
	{
		LOG_ERR("raw rtc init err %d", rc);
	}    


    //Set compare channel to trigger interrupt after COMPARE_COUNTERTIME seconds
    rc = nrfx_rtc_cc_set(&rtc, 0, COMPARE_COUNTERTIME * 8, true);
	if (rc != NRFX_SUCCESS)
	{
		LOG_ERR("nrfx_rtc_cc_set err %d", rc);
	}

	
    //Power on RTC instance
    nrfx_rtc_enable(&rtc);

	nrf_gpio_pin_clear(LED1);
	nrf_gpio_cfg_output(LED1);
	
}

void raw_nrfx_thread(void)
{	
	
	LOG_INF("**Raw SPIM example working with nRF5_SDK\\examples\\peripheral\\spis directly");	
	LOG_WRN("### make sure CONFIG_SPI is not set; otherwise it would have conflicts ###");
	raw_spi_init();
	raw_rtc_init();
    k_sem_take(&sem_raw_nrfx_txrx, K_FOREVER);
	while (1) {                
        LOG_INF("raw spi master thread");
        spi_data_exchange();
        k_sleep(K_MSEC(200)); 
	}
}

K_THREAD_DEFINE(raw_nrfx_thread_id, 1024, raw_nrfx_thread, NULL, NULL,
		NULL, 7, 0, 0);