/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <stdio.h>
#include <modem/lte_lc.h>
#include <net/socket.h>
#include <nrf_socket.h>
#include <modem/at_cmd.h>
#include <sys/reboot.h>

#define UDP_IP_HEADER_SIZE 28

static int client_fd;
static struct sockaddr_storage host_addr;
static struct k_work_delayable server_transmission_work;

static int64_t gs_uptime_ref;
static bool gs_clear_modem_reset_flag = false;
static bool gs_socket_transmit_started = false;

K_SEM_DEFINE(lte_connected, 0, 1);

static int server_init(void);
static void modem_connect(void);
static int server_connect(void);
static void server_disconnect(void);

/* 
	local_rai_req, this RAI is required to use socket option instead of XRAI command. 
*/
int socket_option_rai_req(bool enable)
{
	int err;

	if (enable) {
		err = at_cmd_write("AT%RAI=1", NULL, 0, NULL);
	} else {
		err = at_cmd_write("AT%RAI=0", NULL, 0, NULL);
	}

	return err;
}

/* %XMODEMSLEEP  */
int modem_sleep_report_req(bool enable)
{

	int err;

	if (enable) {
		err = at_cmd_write("AT%XMODEMSLEEP=1,0,10240", NULL, 0, NULL);
	} else {
		err = at_cmd_write("AT%XMODEMSLEEP=0", NULL, 0, NULL);
	}

	return err;

}


int socket_option_rel14_req(bool enable)
{
	int err;

	if (enable) {
		err = at_cmd_write("AT%REL14FEAT=1,1,0,0,1", NULL, 0, NULL);
	} else {
		err = at_cmd_write("AT%REL14FEAT=1,0,0,0,1", NULL, 0, NULL);
	}

	return err;

}

int deep_search_config(bool enable)
{
	int err;

	if (enable) {
		err = at_cmd_write("AT%XDEEPSEARCH=1", NULL, 0, NULL);
	} else {
		err = at_cmd_write("AT%XDEEPSEARCH=0", NULL, 0, NULL);
	}

	return err;

}
/* 
	0, ultra-low power
	1,
	2, default
	3,
	4,
*/
int data_profile_config(void)
{
	int err;
	err = at_cmd_write("AT%XDATAPRFL=0", NULL, 0, NULL);
	return err;

}

int sub_modem_ev(void)
{
	int err;
	err = at_cmd_write("AT%MDMEV=1", NULL, 0, NULL);
	return err;

}

int modem_clear_reset_loop(void)
{
	int err;
	err = at_cmd_write("AT+CFUN=4", NULL, 0, NULL);
	if(err){
		printk("AT+CFUN=4 error: %d\n", err);
	}
	k_sleep(K_MSEC(1000));

	err = at_cmd_write("AT%XFACTORYRESET=1", NULL, 0, NULL);
	if(err){
		printk("XFACTORYRESET=1 error: %d\n", err);
	}
#if 0 /* no need, as sys reboot will be called later.  */
	k_sleep(K_MSEC(1000));

	err = at_cmd_write("AT+CFUN=1", NULL, 0, NULL);
	if(err){
		printk("AT+CFUN=1 error: %d\n", err);
	}
#endif
	return err;
}

static void server_transmission_work_fn(struct k_work *work)
{
	int err;
	static int32_t cnt = 0;
	int32_t len = 0;
	char buffer[CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES] = {"\0"};

	gs_socket_transmit_started = true;
	gs_uptime_ref = k_uptime_get();
#if defined(CONFIG_NRF_MODEM_LIB)
	modem_connect();
	while(1){
		if( k_sem_take(&lte_connected, K_SECONDS(5)) != 0){
			if(gs_clear_modem_reset_flag){
				gs_clear_modem_reset_flag = false;
				modem_clear_reset_loop();   /* hi, please only enable this during development, or debugging.  */
				k_sleep(K_SECONDS(2));
				printk("############ now reboot !\n");
				sys_reboot(SYS_REBOOT_WARM);
				/* only for development, recover if entered resetloop*/
			}
		}
		else{
			break;
		}
	}
	
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

	printk("Transmitting UDP/IP payload of %d bytes to the ",
	       CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES + UDP_IP_HEADER_SIZE);
	printk("IP address %s, port number %d\n",
	       CONFIG_UDP_SERVER_ADDRESS_STATIC,
	       CONFIG_UDP_SERVER_PORT);
	cnt = 0;
	while(cnt < CONFIG_UDP_TOTAL_DATA_KB){  /* moddify here for total packets n x K byes.  */

		/* clear the socket buffer in case some junk in the buffer */
		memset(buffer, 0, sizeof(buffer));
		while( 0 < (len = recv(client_fd, buffer, CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES, MSG_DONTWAIT))  ){
			printk("########## recv before send: %s, len: %d ###########\n", buffer, len);
			memset(buffer, 0, sizeof(buffer));
		}
		/* prepare the sample packet */
		memset(buffer, '#', sizeof(buffer));
		buffer[sizeof(buffer) - 1] = 0; /* set string end */
		sprintf(buffer, "pkg: %d", 	++cnt);
		buffer[strlen(buffer)] = ' ';   /* remove the 0 at endf of pkge: xx  */
		

		err = send(client_fd, buffer, sizeof(buffer), 0);
		if (err < 0) {
			printk("Failed to transmit UDP packet, %d\n", errno);
			goto workerror;
			/* return; */ /* do not return */
		}
		/* prepare to receive the packet */
		memset(buffer, 0, sizeof(buffer));
		/* need to setup socekt timeout option, will block if not  */
		err = recv(client_fd, buffer, CONFIG_UDP_DATA_UPLOAD_SIZE_BYTES, 0);
		if (err < 0) {
			printk("Failed to receive UDP packet, %d\n", errno);
			goto workerror;
			/* return; */ /* do not return */
		}  
		printk("#######recv: %s\n", buffer);


	}
	
workerror:
	server_disconnect();
	err = lte_lc_offline();
	if(err){
		printk("lte_lc_offline error, %d\n", err);
	}
	k_work_schedule(&server_transmission_work,
			K_SECONDS(CONFIG_UDP_DATA_UPLOAD_FREQUENCY_SECONDS));
}

static void work_init(void)
{
	k_work_init_delayable(&server_transmission_work,
			      server_transmission_work_fn);
}

#if defined(CONFIG_NRF_MODEM_LIB)
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		     (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			printk("### Modem not attached to network, status: %d\n", evt->nw_reg_status);
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
		if(evt->rrc_mode == LTE_LC_RRC_MODE_IDLE){
			if(gs_socket_transmit_started == true){
				gs_socket_transmit_started = false;  /* only measure the time if the transmission started */
				printk("\n############### modem active duration: %d ms ###########\n", (int32_t)k_uptime_delta(&gs_uptime_ref)); 
			}
		}
		printk("RRC mode: %s\n",
			evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
			"Connected" : "Idle\n");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
		       evt->cell.id, evt->cell.tac);
		break;
	case LTE_LC_EVT_MODEM_SLEEP_ENTER: /* 10 */
	case LTE_LC_EVT_MODEM_SLEEP_EXIT: /* 9 */
	case LTE_LC_EVT_MODEM_SLEEP_EXIT_PRE_WARNING: /* 8 */

		printk("### LTE Modem Sleep evt = %d\n", evt->type);
		break;
	case LTE_LC_EVT_MODEM_EVENT: /* 11 */
		if( evt->modem_evt == LTE_LC_MODEM_EVT_RESET_LOOP){
			/* reset loop detected */
			printk("### Modem reset loop detected = %d\n", evt->modem_evt);
			gs_clear_modem_reset_flag = true; /*  only for debugging, not suggest to do so if it's actuall reset loop caused by battery or power source low */

		}else{
			printk("### other modem event = %d\n",  evt->modem_evt); /* light_search_done, search_done, reset_loop, battery_low, over_heat  */
		}
		break;	
	default:
		printk("##### other event type: %d\n", evt->type); /* check lte_lc.h for details.  */
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
#if 0 /* skip this one, as socket option used */
#if defined(CONFIG_UDP_RAI_ENABLE)
	/** Release Assistance Indication  */
	err = lte_lc_rai_req(true);
	if (err) {
		printk("lte_lc_rai_req, error: %d\n", err);
	}
#endif
#endif /* #if 0*/

	err =  socket_option_rel14_req(true);
	if (err) {
		printk("socket_option_rel14_req, error: %d\n", err);
	}

	err =  socket_option_rai_req(true);
	if (err) {
		printk("socket_option_rai_req, error: %d\n", err);
	}
	err = modem_sleep_report_req(true);
	if(err) {
		printk("modem_sleep_report_req, error: %d\n", err);
	}
	err = data_profile_config();
	if(err) {
		printk("data_profile_config, error: %d\n", err);
	}

	err = deep_search_config(false);
	if(err) {
		printk("deep_search_config, error: %d\n", err);
	}
	err = sub_modem_ev();
	if(err) {
		printk("sub_modem_ev, error: %d\n", err);
	}
	
	return err;
}

static void modem_init(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_init();
		if (err) {
			printk("Modem initialization failed, error: %d\n", err);
			return;
		}
	}
}

static void modem_connect(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_connect_async(lte_handler);
		if (err) {
			printk("Connecting to LTE network failed, error: %d\n",
			       err);
			return;
		}
	}
}
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
		err = -errno;
		goto error;
	}
#if defined(CONFIG_UDP_RECV_TIMEOUT_SECONDS)	
	/* setup socket timeout */
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

void main(void)
{
	int err;

	printk("UDP sample has started\n");

	work_init();

#if defined(CONFIG_NRF_MODEM_LIB)

	/* Initialize the modem before calling configure_low_power(). This is
	 * because the enabling of RAI is dependent on the
	 * configured network mode which is set during modem initialization.
	 */
	modem_init();

	err = configure_low_power();
	if (err) {
		printk("Unable to set low power configuration, error: %d\n",
		       err);
	}
#endif
	k_work_schedule(&server_transmission_work, K_NO_WAIT);
}
