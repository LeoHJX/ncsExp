/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <modem/lte_lc.h>
#include <net/socket.h>
#include <modem/at_cmd.h>
#define UDP_IP_HEADER_SIZE 28

static int client_fd;
static struct sockaddr_storage host_addr;
static struct k_delayed_work server_transmission_work;

K_SEM_DEFINE(lte_connected, 0, 1);

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
			       "Connected - home network" :
			       "Connected - roaming\n");
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
			       "Connected" :
			       "Idle\n");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
		       evt->cell.id, evt->cell.tac);
		break;
	default:
		break;
	}
}
#if !defined(CONFIG_UDP_ON_OFF_ENABLE) 
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
#endif /*#if defined(CONFIG_UDP_ON_OFF_ENABLE)  */
/*   new modem_configure with init and connect async, error on RAI request if default setup is CATM1 network. */

int lte_lc_init_and_connect_async_new(lte_lc_evt_handler_t handler)
{
	int err;

	err = lte_lc_init();
	if (err) {
		return err;
	}
#if !defined(CONFIG_UDP_ON_OFF_ENABLE) 
	err = configure_low_power(); /* setup low power before connect */
	if (err) {
		printk("Unable to set low power configuration, error: %d\n",
		       err);
	}
#endif
	return lte_lc_connect_async(handler);
}

#if defined(CONFIG_UDP_ON_OFF_ENABLE) 
int lte_lc_init_and_connect_async_onoff(void)
{
	int err;

	err = lte_lc_init();
	if (err) {
		return err;
	}
#if !defined(CONFIG_UDP_ON_OFF_ENABLE) 
	err = configure_low_power(); /* setup low power before connect */
	if (err) {
		printk("Unable to set low power configuration, error: %d\n",
		       err);
	}
#endif
    return err;
	
}

int lte_lc_connect_async_onoff(lte_lc_evt_handler_t handler)
{
    return lte_lc_connect_async(handler);
}

static void modem_configure_onoff(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_init_and_connect_async_onoff();
		if (err) {
			printk("Modem configuration, error: %d\n", err);
			return;
		}
	}
}

#endif
#if !defined(CONFIG_UDP_ON_OFF_ENABLE) 
static void modem_configure(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_init_and_connect_async_new(lte_handler);
		if (err) {
			printk("Modem configuration, error: %d\n", err);
			return;
		}
	}
}
#endif /*  #if !defined(CONFIG_UDP_ON_OFF_ENABLE)   */
#endif

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

int lte_lc_cereg_setup_onoff(void)
{
	if (at_cmd_write("AT+CEREG=5", NULL, 0, NULL) != 0) {
		return -EIO;
	}
	return 0;
}

static void server_transmission_work_fn(struct k_work *work)
{
	int err;
	char buffer[CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES] = { "\0" };

#if defined(CONFIG_UDP_ON_OFF_ENABLE) 

#if defined(CONFIG_BSD_LIBRARY)
    lte_lc_connect_async_onoff(lte_handler);
	k_sem_take(&lte_connected, K_FOREVER);
#endif
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
#endif
	printk("Transmitting UDP/IP payload of %d bytes to the ",
	       CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES + UDP_IP_HEADER_SIZE);
	printk("IP address %s, port number %d\n",
	       CONFIG_UDP_SERVER_ADDRESS_STATIC, CONFIG_UDP_SERVER_PORT);

	err = send(client_fd, buffer, sizeof(buffer), 0);
	if (err < 0) {
		printk("Failed to transmit UDP packet, %d\n", errno);
		return;
	}
#if defined(CONFIG_UDP_ON_OFF_ENABLE) 

    lte_lc_offline(); 
    /*lte_lc_power_off(); */
#endif 
    server_disconnect();
    /* lte_lc_cereg_setup_onoff(); */   /* Have to enable this line if use CFUN=0 instead of CFUN=4.  */

	k_delayed_work_submit(
		&server_transmission_work,
		K_SECONDS(CONFIG_UDP_DATA_UPLOAD_FREQUENCY_SECONDS));
}

static void work_init(void)
{
	k_delayed_work_init(&server_transmission_work,
			    server_transmission_work_fn);
}
void disable_uart() // disable UART to measure current.
{
	NRF_UARTE0->ENABLE = 0;
	NRF_UARTE1->ENABLE = 0;
}

void main(void)
{
#if !defined(CONFIG_UDP_ON_OFF_ENABLE) 
	int err;
#endif
	printk("UDP sample has started\n");

	work_init();

#if defined(CONFIG_UDP_ON_OFF_ENABLE) 
    modem_configure_onoff();

#else
#if defined(CONFIG_BSD_LIBRARY)

	modem_configure(); /* move modem config before low power config, RAI won't work if the LTE network mode by default not NBIoT  */

	k_sem_take(&lte_connected, K_FOREVER);
#endif



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
#endif
	k_delayed_work_submit(&server_transmission_work, K_NO_WAIT);
	/* disable UART here to save power */

	//disable_uart();
}
