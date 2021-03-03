/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <drivers/gpio.h>
#include <drivers/flash.h>
#include <bsd.h>
#include <modem/lte_lc.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include <modem/bsdlib.h>
#include <modem/modem_key_mgmt.h>
#include <net/fota_download.h>
#include <dfu/mcuboot.h>
#include <net/socket.h>

#include <string.h>
#include <stdlib.h>
#include <sys/printk.h>
#include <drivers/uart.h>

#define UART_DEV_NAME "UART_0"

/*  various testing options */

#define UART_USE_RX_INTERRUPT 1

#define UART_LP_USE_Z_PWR_MANAGER 0  /* not used in this example */


/*   testing options end */


#define UDP_IP_HEADER_SIZE 28
#define APP_HTTP_UPDATE 1



static int client_fd;
static struct sockaddr_storage host_addr;
static struct k_delayed_work server_transmission_work;

#if APP_HTTP_UPDATE

#define LED_PORT	DT_GPIO_LABEL(DT_ALIAS(led0), gpios)
#define TLS_SEC_TAG 42

static const struct	device *gpiob;
static struct		gpio_callback gpio_cb;
static struct k_work	fota_work;
int cert_provision(void);
#endif


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



void uart_echo_start(void)
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

#if UART_LP_USE_Z_PWR_MANAGER
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


	/*  looping with non-interrupt */
#if !UART_USE_RX_INTERRUPT /* enable echo with polling mode, don't use in this example, which will block others. */
	while (1) {
		uint8_t temp;
		while (uart_fifo_read(uart_dev, &temp, 1)) {
			uart_poll_out(uart_dev, temp);
		}
	}
#endif
}


K_SEM_DEFINE(lte_connected, 0, 1);

static void server_transmission_work_fn(struct k_work *work)
{
	int err;
	char buffer[CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES] = {"\0"};

	printk("Transmitting UDP/IP payload of %d bytes to the ",
	       CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES + UDP_IP_HEADER_SIZE);
	printk("IP address %s, port number %d\n",
	       CONFIG_UDP_SERVER_ADDRESS_STATIC,
	       CONFIG_UDP_SERVER_PORT);

	err = send(client_fd, buffer, sizeof(buffer), 0);
	if (err < 0) {
		printk("Failed to transmit UDP packet, %d\n", errno);
		return;
	}

	k_delayed_work_submit(
			&server_transmission_work,
			K_SECONDS(CONFIG_UDP_DATA_UPLOAD_FREQUENCY_SECONDS));
}

static void work_init(void)
{
	k_delayed_work_init(&server_transmission_work,
			    server_transmission_work_fn);
}

#if defined(CONFIG_BSD_LIBRARY)
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		     (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			break;
		}

		printk("Network registration status: %s\n",
			evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
			"Connected - home network" : "Connected - roaming\n");
		k_sem_give(&lte_connected);
		break;
	case LTE_LC_EVT_PSM_UPDATE:
		printk("PSM parameter update: TAU: %d, Active time: %d\n",
			evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;
	case LTE_LC_EVT_EDRX_UPDATE: {
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
			       "eDRX parameter update: eDRX: %f, PTW: %f\n",
			       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0) {
			printk("%s\n", log_buf);
		}
		break;
	}
	case LTE_LC_EVT_RRC_UPDATE:
		printk("RRC mode: %s\n",
			evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
			"Connected" : "Idle\n");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
		       evt->cell.id, evt->cell.tac);
		break;
	default:
		break;
	}
}

static int configure_low_power(void)
{
	int err;

#if defined(CONFIG_UDP_PSM_ENABLE)
	/** Power Saving Mode */
	err = lte_lc_psm_req(true);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#else
	err = lte_lc_psm_req(false);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_UDP_EDRX_ENABLE)
	/** enhanced Discontinuous Reception */
	err = lte_lc_edrx_req(true);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#else
	err = lte_lc_edrx_req(false);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_UDP_RAI_ENABLE)
	/** Release Assistance Indication  */
	err = lte_lc_rai_req(true);
	if (err) {
		printk("lte_lc_rai_req, error: %d\n", err);
	}
#endif

	return err;
}

static void modem_configure(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
#if APP_HTTP_UPDATE
#if !defined(CONFIG_BSD_LIBRARY_SYS_INIT)
		/* Initialize AT only if bsdlib_init() is manually
             * called by the main application
             */
		err = at_notif_init();
		__ASSERT(err == 0, "AT Notify could not be initialized.");
		//err = at_cmd_init();
		// __ASSERT(err == 0, "AT CMD could not be established.");
#endif /* !defined(CONFIG_BSD_LIBRARY_SYS_INIT) */
#if defined(CONFIG_USE_HTTPS)
		err = cert_provision();
		__ASSERT(err == 0, "Could not provision root CA to %d",
			 TLS_SEC_TAG);
#endif /* defined(CONFIG_USE_HTTPS) */
#endif /* APP_HTTP_UPDATE */
		err = lte_lc_init_and_connect_async(lte_handler);
		if (err) {
			printk("Modem configuration, error: %d\n", err);
			return;
		}
	}
}
#endif /* defined(CONFIG_BSD_LIBRARY)  */

static void server_disconnect(void)
{
	(void)close(client_fd);
}

static int server_init(void)
{
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&host_addr);

	server4->sin_family = AF_INET;
	server4->sin_port = htons(CONFIG_UDP_SERVER_PORT);

	inet_pton(AF_INET, CONFIG_UDP_SERVER_ADDRESS_STATIC,
		  &server4->sin_addr);

	return 0;
}

static int server_connect(void)
{
	int err;

	client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client_fd < 0) {
		printk("Failed to create UDP socket: %d\n", errno);
		goto error;
	}

	err = connect(client_fd, (struct sockaddr *)&host_addr,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		printk("Connect failed : %d\n", errno);
		goto error;
	}

	return 0;

error:
	server_disconnect();

	return err;
}


#if APP_HTTP_UPDATE

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t err)
{
	printk("bsdlib recoverable error: %u\n", err);
}

int cert_provision(void)
{
	static const char cert[] = {
		#include "../cert/BaltimoreCyberTrustRoot"
	};
	BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");
	int err;
	bool exists;
	uint8_t unused;

	err = modem_key_mgmt_exists(TLS_SEC_TAG,
				    MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    &exists, &unused);
	if (err) {
		printk("Failed to check for certificates err %d\n", err);
		return err;
	}

	if (exists) {
		/* For the sake of simplicity we delete what is provisioned
		 * with our security tag and reprovision our certificate.
		 */
		err = modem_key_mgmt_delete(TLS_SEC_TAG,
					    MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		if (err) {
			printk("Failed to delete existing certificate, err %d\n",
			       err);
		}
	}

	printk("Provisioning certificate\n");

	/*  Provision certificate to the modem */
	err = modem_key_mgmt_write(TLS_SEC_TAG,
				   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				   cert, sizeof(cert) - 1);
	if (err) {
		printk("Failed to provision certificate, err %d\n", err);
		return err;
	}

	return 0;
}

/**@brief Start transfer of the file. */
static void app_dfu_transfer_start(struct k_work *unused)
{
	int retval;
	int sec_tag;
	char *apn = NULL;

#ifndef CONFIG_USE_HTTPS
	sec_tag = -1;
#else
	sec_tag = TLS_SEC_TAG;
#endif

	retval = fota_download_start(CONFIG_DOWNLOAD_HOST,
				     CONFIG_DOWNLOAD_FILE,
				     sec_tag,
				     apn,
				     0);
	if (retval != 0) {
		/* Re-enable button callback */
		gpio_pin_interrupt_configure(gpiob,
					     DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
					     GPIO_INT_EDGE_TO_ACTIVE);

		printk("fota_download_start() failed, err %d\n",
			retval);
	}

}

/**@brief Turn on LED0 and LED1 if CONFIG_APPLICATION_VERSION
 * is 2 and LED0 otherwise.
 */
static int led_app_version(void)
{
	const struct device *dev;

	dev = device_get_binding(LED_PORT);
	if (dev == 0) {
		printk("Nordic nRF GPIO driver was not found!\n");
		return 1;
	}

	gpio_pin_configure(dev, DT_GPIO_PIN(DT_ALIAS(led0), gpios),
			   GPIO_OUTPUT_ACTIVE |
			   DT_GPIO_FLAGS(DT_ALIAS(led0), gpios));

#if CONFIG_APPLICATION_VERSION == 2
	gpio_pin_configure(dev, DT_GPIO_PIN(DT_ALIAS(led1), gpios),
			   GPIO_OUTPUT_ACTIVE |
			   DT_GPIO_FLAGS(DT_ALIAS(led1), gpios));
#endif
	return 0;
}

void dfu_button_pressed(const struct device *gpiob, struct gpio_callback *cb,
			uint32_t pins)
{
	k_work_submit(&fota_work);
	gpio_pin_interrupt_configure(gpiob, DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
				     GPIO_INT_DISABLE);
}

static int dfu_button_init(void)
{
	int err;

	gpiob = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(sw0), gpios));
	if (gpiob == 0) {
		printk("Nordic nRF GPIO driver was not found!\n");
		return 1;
	}
	err = gpio_pin_configure(gpiob, DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
				 GPIO_INPUT |
				 DT_GPIO_FLAGS(DT_ALIAS(sw0), gpios));
	if (err == 0) {
		gpio_init_callback(&gpio_cb, dfu_button_pressed,
			BIT(DT_GPIO_PIN(DT_ALIAS(sw0), gpios)));
		err = gpio_add_callback(gpiob, &gpio_cb);
	}
	if (err == 0) {
		err = gpio_pin_interrupt_configure(gpiob,
						   DT_GPIO_PIN(DT_ALIAS(sw0),
							       gpios),
						   GPIO_INT_EDGE_TO_ACTIVE);
	}
	if (err != 0) {
		printk("Unable to configure SW0 GPIO pin!\n");
		return 1;
	}
	return 0;
}


void fota_dl_handler(const struct fota_download_evt *evt)
{
	switch (evt->id) {
	case FOTA_DOWNLOAD_EVT_ERROR:
		printk("Received error from fota_download\n");
		/* Fallthrough */
	case FOTA_DOWNLOAD_EVT_FINISHED:
		/* Re-enable button callback */
		gpio_pin_interrupt_configure(gpiob,
					     DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
					     GPIO_INT_EDGE_TO_ACTIVE);
		break;

	default:
		break;
	}
}


static int application_init(void)
{
	int err;

	k_work_init(&fota_work, app_dfu_transfer_start);

	err = dfu_button_init();
	if (err != 0) {
		return err;
	}

	err = led_app_version();
	if (err != 0) {
		return err;
	}

	err = fota_download_init(fota_dl_handler);
	if (err != 0) {
		return err;
	}

	return 0;
}

void ota_svr_start(void)
{
	int err;

	printk("HTTP application update sample started\n");
	printk("Initializing bsdlib\n");
#if !defined(CONFIG_BSD_LIBRARY_SYS_INIT)
	err = bsdlib_init();
#else
	/* If bsdlib is initialized on post-kernel we should
	 * fetch the returned error code instead of bsdlib_init
	 */
	err = bsdlib_get_init_ret();
#endif
	switch (err) {
	case MODEM_DFU_RESULT_OK:
		printk("Modem firmware update successful!\n");
		printk("Modem will run the new firmware after reboot\n");
		k_thread_suspend(k_current_get());
		break;
	case MODEM_DFU_RESULT_UUID_ERROR:
	case MODEM_DFU_RESULT_AUTH_ERROR:
		printk("Modem firmware update failed\n");
		printk("Modem will run non-updated firmware on reboot.\n");
		break;
	case MODEM_DFU_RESULT_HARDWARE_ERROR:
	case MODEM_DFU_RESULT_INTERNAL_ERROR:
		printk("Modem firmware update failed\n");
		printk("Fatal error.\n");
		break;
	case -1:
		printk("Could not initialize bsdlib.\n");
		printk("Fatal error.\n");
		return;
	default:
		break;
	}
	printk("Initialized bsdlib\n");

	/* modem_configure(); */ // been initilized.

	boot_write_img_confirmed();

	err = application_init();
	if (err != 0) {
		return;
	}

	printk("Press Button 1 to start the FOTA download\n");
}

#endif


void main(void)
{
	int err;

	printk("UDP sample has started\n");

	work_init();

#if defined(CONFIG_BSD_LIBRARY)
	err = configure_low_power();
	if (err) {
		printk("Unable to set low power configuration, error: %d\n",
		       err);
	}

	modem_configure();

	k_sem_take(&lte_connected, K_FOREVER);
#endif
/////////////////////UDP start ///////////////////////////////////////
	err = server_init();
	if (err) {
		printk("Not able to initialize UDP server connection\n");
		return;
	}

	err = server_connect();
	if (err) {
		printk("Not able to connect to UDP server\n");
		return;
	}

	k_delayed_work_submit(&server_transmission_work, K_NO_WAIT);

/////////////////UDP end here//////////////////////////////////////////

#if APP_HTTP_UPDATE
    ota_svr_start();  /* start the ota services */ 
#endif
	uart_echo_start();
}
