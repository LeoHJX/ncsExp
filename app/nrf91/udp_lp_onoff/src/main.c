/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <modem/lte_lc.h>
#include <nrf_socket.h>
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

/*   new modem_configure with init and connect async, error on RAI request if default setup is CATM1 network. */

int lte_lc_init_and_connect_async_new(lte_lc_evt_handler_t handler)
{
	int err;

	err = lte_lc_init();
	if (err) {
		return err;
	}

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

#endif /* #if defined(CONFIG_UDP_ON_OFF_ENABLE)   */
#endif /* #if defined(CONFIG_BSD_LIBRARY)  */

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
#if defined(CONFIG_TEST_USE_TCP)
	client_fd = socket(AF_INET, SOCK_STREAM /* SOCK_DGRAM */, IPPROTO_TCP /* IPPROTO_UDP*/);
#else
	client_fd = socket(AF_INET,  SOCK_DGRAM , IPPROTO_UDP);
#endif
	if (client_fd < 0) {
		printk("Failed to create UDP socket: %d\n", errno);
		goto error;
	}
#if defined(CONFIG_UDP_RECV_TIMEOUT_SECONDS)
	struct timeval timeout = {
		.tv_sec = CONFIG_UDP_RECV_TIMEOUT_SECONDS,
		.tv_usec = 0,
	};

	err = setsockopt(client_fd,
			 NRF_SOL_SOCKET,
			 NRF_SO_RCVTIMEO,
			 &timeout,
			 sizeof(timeout));
	if (err) {
		printk("Failed to setup socket timeout, errno %d\n", errno);
		/* do not return */ /* return -1; */
	}
#endif
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

/*  setup CEREG=5 for URC, only require this fun call if CFUN=0 used to switch modem off */
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
    static uint32_t cnt;

	char buffer[CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES + 8] = {"#"}; /* reserve data at the end of the buffer */

    memset(buffer, '#', CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES);
#if defined(CONFIG_RECEIVE_UDP_PACKETS)
    cnt=0;
    sprintf(buffer, "pkg:%d ", cnt);  /* fixed packets for server to check and reply.  */
#else
    sprintf(buffer, "pkg:%d ", cnt++);
#endif
    

#if defined(CONFIG_UDP_ON_OFF_ENABLE) 

#if defined(CONFIG_BSD_LIBRARY)
    lte_lc_connect_async_onoff(lte_handler);
	k_sem_take(&lte_connected, K_FOREVER);
#endif
	err = server_init();
	if (err) {
		printk("Not able to initialize UDP server connection\n");
        goto workerror;
		/* return; */ /* do not return */
	}
	err = server_connect();
	if (err) {
		printk("Not able to connect to UDP server\n");
        goto workerror;
		/* return; */ /* do not return */
	}
#endif
	printk("Transmitting UDP/IP payload of %d bytes to the ",
	       CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES + UDP_IP_HEADER_SIZE);
	printk("IP address %s, port number %d\n",
	       CONFIG_UDP_SERVER_ADDRESS_STATIC, CONFIG_UDP_SERVER_PORT);

	err = send(client_fd, buffer, CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES, 0);
	if (err < 0) {
		printk("Failed to transmit UDP packet, %d\n", errno);
        goto workerror;
		/* return; */ /* do not return */
	}
#if defined(CONFIG_RECEIVE_UDP_PACKETS)

    memset(buffer, 0, sizeof(buffer));

	err = recv(client_fd, buffer, CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES, 0);
	if (err < 0) {
		printk("Failed to receive UDP packet, %d\n", errno);
        goto workerror;
		/* return; */ /* do not return */
	}  
    printk("recv: %s\n", buffer);

#endif

workerror:

    server_disconnect();
#if defined(CONFIG_UDP_ON_OFF_ENABLE) 

#if defined(CONFIG_USE_CFUN_0_OFF)
	lte_lc_power_off(); 
#else
	lte_lc_offline();
#endif
     
#endif 
    

#if defined(CONFIG_USE_CFUN_0_OFF)	
    lte_lc_cereg_setup_onoff();   /**/   /* Have to enable this line if use CFUN=0 instead of CFUN=4.  */
#endif
	k_delayed_work_submit(
		&server_transmission_work,
		K_SECONDS(CONFIG_UDP_DATA_UPLOAD_FREQUENCY_SECONDS));
}

static void work_init(void)
{
	k_delayed_work_init(&server_transmission_work,
			    server_transmission_work_fn);
}
void disable_uart() /* disable UART to measure current. use this one if CONFIG_SERIAL=n doens't work */
{
	NRF_UARTE0->ENABLE = 0;
	NRF_UARTE1->ENABLE = 0;
}

void main(void)
{
	printk("UDP sample has started\n");

	work_init();

#if defined(CONFIG_UDP_ON_OFF_ENABLE) 
    modem_configure_onoff();
#endif
	k_delayed_work_submit(&server_transmission_work, K_NO_WAIT);

	/* no need for now,  disable UART here to save power */
	/*  disable_uart();  */ 
}
