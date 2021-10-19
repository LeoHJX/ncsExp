/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Service Client sample
 */

#include <errno.h>
#include <zephyr.h>
#include <sys/byteorder.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <bluetooth/services/nus.h>
#include <bluetooth/services/nus_client.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>

#include <dk_buttons_and_leds.h>

#include <settings/settings.h>

#include <drivers/uart.h>

#include <logging/log.h>

#define LOG_MODULE_NAME central_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define STACKSIZE 4096
#define PRIORITY 7

/* UART payload buffer element size. */
#define UART_BUF_SIZE 20

#define KEY_PASSKEY_ACCEPT DK_BTN1_MSK
#define KEY_PASSKEY_REJECT DK_BTN2_MSK

#define KEY_THROUGHPUT_SEND     DK_BTN3_MSK
#define KEY_UPDATE_BLE_PARAMS   DK_BTN4_MSK
#define NUS_WRITE_TIMEOUT K_MSEC(150)
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_RX_TIMEOUT 50

#define THROUGHPUT_PACKETS_TO_SEND  (1000)
#define NUMBER_THROUGHPUT_TESTS  (10)
#define THROUGH_PACKET_SIZE 244
#define RECV_THRESHOLD_CNT 100
#define RECV_THRESHOLD (THROUGH_PACKET_SIZE * RECV_THRESHOLD_CNT)
#define NUM_SPEED_CACULATIONS (THROUGHPUT_PACKETS_TO_SEND/RECV_THRESHOLD_CNT)


static volatile bool data_length_req;

static const struct device *uart;
static struct k_work_delayable uart_work;

static struct k_work_delayable ble_params_update;

//K_SEM_DEFINE(nus_write_sem, 0, 1);
static K_SEM_DEFINE(throughput_sem, 0, 1);

struct uart_data_t {
	void *fifo_reserved;
	uint8_t  data[UART_BUF_SIZE];
	uint16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static struct bt_conn *default_conn;
static struct bt_nus_client nus_client;

static void ble_params_update_work_handler(struct k_work *item);


static void ble_data_sent(uint8_t err, const uint8_t *const data, uint16_t len)
{

#if 0
    struct uart_data_t *buf;

	/* Retrieve buffer context. */
	buf = CONTAINER_OF(data, struct uart_data_t, data);
	k_free(buf);

	k_sem_give(&nus_write_sem);

	if (err) {
		LOG_WRN("ATT error code: 0x%02X", err);
	}
#endif
}


void button_changed(uint32_t button_state, uint32_t has_changed)
{
	uint32_t buttons = button_state & has_changed;

    if(buttons & KEY_UPDATE_BLE_PARAMS){
        k_work_reschedule(&ble_params_update, K_MSEC(10));
    }

    if(buttons & KEY_THROUGHPUT_SEND){
       k_sem_give(&throughput_sem);
    }
}
static void configure_gpio(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
}

uint64_t bytes_received = 0;
uint32_t pre_timestamp = 0;
uint32_t cur_timestamp = 0;

/* 

#define THROUGHPUT_PACKETS_TO_SEND  (1000)
#define NUMBER_THROUGHPUT_TESTS  (10)
#define THROUGH_PACKET_SIZE 244
*/

void recv_data_process(uint16_t len)
{
    uint32_t time_diff = 0;
    static uint32_t throughput_speed_caculation_cnt;

    
    bytes_received += len;

    if(bytes_received%RECV_THRESHOLD == 0 ){
        throughput_speed_caculation_cnt += 1;
        cur_timestamp = k_uptime_get_32();
        time_diff =  cur_timestamp - pre_timestamp;
        pre_timestamp = cur_timestamp;

        LOG_INF("time %d ms -- received: %u", cur_timestamp, (uint32_t) (bytes_received));
        if((throughput_speed_caculation_cnt % NUM_SPEED_CACULATIONS) != 1){ /* skip the first caculation, as unknow starting time stamp */
            LOG_INF("recv speed %d kbps", (RECV_THRESHOLD * 8)/time_diff);
        }

    }
}
static uint8_t ble_data_received(const uint8_t *const data, uint16_t len)
{
    recv_data_process(len);
#if 0
	int err;

	for (uint16_t pos = 0; pos != len;) {
		struct uart_data_t *tx = k_malloc(sizeof(*tx));

		if (!tx) {
			LOG_WRN("Not able to allocate UART send data buffer");
			return BT_GATT_ITER_CONTINUE;
		}

		/* Keep the last byte of TX buffer for potential LF char. */
		size_t tx_data_size = sizeof(tx->data) - 1;

		if ((len - pos) > tx_data_size) {
			tx->len = tx_data_size;
		} else {
			tx->len = (len - pos);
		}

		memcpy(tx->data, &data[pos], tx->len);

		pos += tx->len;

		/* Append the LF character when the CR character triggered
		 * transmission from the peer.
		 */
		if ((pos == len) && (data[len - 1] == '\r')) {
			tx->data[tx->len] = '\n';
			tx->len++;
		}

		err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
		if (err) {
			k_fifo_put(&fifo_uart_tx_data, tx);
		}
	}
#endif
	return BT_GATT_ITER_CONTINUE;
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);

	static uint8_t *current_buf;
	static size_t aborted_len;
	static bool buf_release;
	struct uart_data_t *buf;
	static uint8_t *aborted_buf;

	switch (evt->type) {
	case UART_TX_DONE:
		if ((evt->data.tx.len == 0) ||
		    (!evt->data.tx.buf)) {
			return;
		}

		if (aborted_buf) {
			buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
					   data);
			aborted_buf = NULL;
			aborted_len = 0;
		} else {
			buf = CONTAINER_OF(evt->data.tx.buf,
					   struct uart_data_t,
					   data);
		}

		k_free(buf);

		buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
		if (!buf) {
			return;
		}

		if (uart_tx(uart, buf->data, buf->len, SYS_FOREVER_MS)) {
			LOG_WRN("Failed to send data over UART");
		}

		break;

	case UART_RX_RDY:
		buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data);
		buf->len += evt->data.rx.len;
		buf_release = false;

		if (buf->len == UART_BUF_SIZE) {
			k_fifo_put(&fifo_uart_rx_data, buf);
		} else if ((evt->data.rx.buf[buf->len - 1] == '\n') ||
			  (evt->data.rx.buf[buf->len - 1] == '\r')) {
			k_fifo_put(&fifo_uart_rx_data, buf);
			current_buf = evt->data.rx.buf;
			buf_release = true;
			uart_rx_disable(uart);
		}

		break;

	case UART_RX_DISABLED:
		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
			k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
			return;
		}

		uart_rx_enable(uart, buf->data, sizeof(buf->data),
			       UART_RX_TIMEOUT);

		break;

	case UART_RX_BUF_REQUEST:
		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
			uart_rx_buf_rsp(uart, buf->data, sizeof(buf->data));
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
		}

		break;

	case UART_RX_BUF_RELEASED:
		buf = CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t,
				   data);
		if (buf_release && (current_buf != evt->data.rx_buf.buf)) {
			k_free(buf);
			buf_release = false;
			current_buf = NULL;
		}

		break;

	case UART_TX_ABORTED:
			if (!aborted_buf) {
				aborted_buf = (uint8_t *)evt->data.tx.buf;
			}

			aborted_len += evt->data.tx.len;
			buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
					   data);

			uart_tx(uart, &buf->data[aborted_len],
				buf->len - aborted_len, SYS_FOREVER_MS);

		break;

	default:
		break;
	}
}
#define INTERVAL_MIN	60	/* x * 1.25 ms */
#define INTERVAL_MAX	60	/* x * 1.25 ms */


static void uart_work_handler(struct k_work *item)
{
	struct uart_data_t *buf;

	buf = k_malloc(sizeof(*buf));
	if (buf) {
		buf->len = 0;
	} else {
		LOG_WRN("Not able to allocate UART receive buffer");
		k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
		return;
	}

	uart_rx_enable(uart, buf->data, sizeof(buf->data), UART_RX_TIMEOUT);
}

static int uart_init(void)
{
	int err;
	struct uart_data_t *rx;

	uart = device_get_binding(DT_LABEL(DT_NODELABEL(uart0)));
	if (!uart) {
		LOG_ERR("UART binding failed");
		return -ENXIO;
	}

	rx = k_malloc(sizeof(*rx));
	if (rx) {
		rx->len = 0;
	} else {
		return -ENOMEM;
	}

	k_work_init_delayable(&uart_work, uart_work_handler);
    
	err = uart_callback_set(uart, uart_cb, NULL);
	if (err) {
		return err;
	}

	return uart_rx_enable(uart, rx->data, sizeof(rx->data),
			      UART_RX_TIMEOUT);
}

static void discovery_complete(struct bt_gatt_dm *dm,
			       void *context)
{
	struct bt_nus_client *nus = context;
	LOG_INF("Service discovery completed");

	bt_gatt_dm_data_print(dm);

	bt_nus_handles_assign(dm, nus);
	bt_nus_subscribe_receive(nus);

	bt_gatt_dm_data_release(dm);
}

static void discovery_service_not_found(struct bt_conn *conn,
					void *context)
{
	LOG_INF("Service not found");
}

static void discovery_error(struct bt_conn *conn,
			    int err,
			    void *context)
{
	LOG_WRN("Error while discovering GATT database: (%d)", err);
}

struct bt_gatt_dm_cb discovery_cb = {
	.completed         = discovery_complete,
	.service_not_found = discovery_service_not_found,
	.error_found       = discovery_error,
};

static void gatt_discover(struct bt_conn *conn)
{
	int err;

	if (conn != default_conn) {
		return;
	}

	err = bt_gatt_dm_start(conn,
			       BT_UUID_NUS_SERVICE,
			       &discovery_cb,
			       &nus_client);
	if (err) {
		LOG_ERR("could not start the discovery procedure, error "
			"code: %d", err);
	}
}

static void exchange_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params)
{
	if (!err) {
		LOG_INF("MTU exchange done");
	} else {
		LOG_WRN("MTU exchange failed (err %" PRIu8 ")", err);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_INF("Failed to connect to %s (%d)", log_strdup(addr),
			conn_err);

		if (default_conn == conn) {
			bt_conn_unref(default_conn);
			default_conn = NULL;

			err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
			if (err) {
				LOG_ERR("Scanning failed to start (err %d)",
					err);
			}
		}

		return;
	}

	LOG_INF("Connected: %s", log_strdup(addr));

	static struct bt_gatt_exchange_params exchange_params;

	exchange_params.func = exchange_func;
	err = bt_gatt_exchange_mtu(conn, &exchange_params);
	if (err) {
		LOG_WRN("MTU exchange failed (err %d)", err);
	}

	err = bt_conn_set_security(conn, BT_SECURITY_L1);
	if (err) {
		LOG_WRN("Failed to set security: %d", err);

		gatt_discover(conn);
	}
    gatt_discover(conn);
    k_work_init_delayable(&ble_params_update, ble_params_update_work_handler);
	err = bt_scan_stop();
	if ((!err) && (err != -EALREADY)) {
		LOG_ERR("Stop LE scan failed (err %d)", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", log_strdup(addr),
		reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)",
			err);
	}
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", log_strdup(addr),
			level);
	} else {
		LOG_WRN("Security failed: %s level %u err %d", log_strdup(addr),
			level, err);
	}

	gatt_discover(conn);
}

static const char *phy2str(uint8_t phy)
{
	switch (phy) {
	case 0: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}


static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	printk("Connection parameters update request received.\n");
	printk("Minimum interval: %d, Maximum interval: %d\n",
	       param->interval_min, param->interval_max);
	printk("Latency: %d, Timeout: %d\n", param->latency, param->timeout);

	return true;
}

static void le_phy_updated(struct bt_conn *conn,
			   struct bt_conn_le_phy_info *param)
{
	printk("LE PHY updated: TX PHY %s, RX PHY %s\n",
	       phy2str(param->tx_phy), phy2str(param->rx_phy));

	//k_sem_give(&throughput_sem);
}

static void le_data_length_updated(struct bt_conn *conn,
				   struct bt_conn_le_data_len_info *info)
{
	if (!data_length_req) {
		return;
	}

	printk("LE data len updated: TX (len: %d time: %d)"
	       " RX (len: %d time: %d)\n", info->tx_max_len,
	       info->tx_max_time, info->rx_max_len, info->rx_max_time);

	data_length_req = false;
	//k_sem_give(&throughput_sem);
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	printk("Connection parameters updated.\n"
	       " interval: %d, latency: %d, timeout: %d\n",
	       interval, latency, timeout);

	//k_sem_give(&throughput_sem);
}


static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
    .le_param_req = le_param_req,
    .le_param_updated = le_param_updated,
    .le_phy_updated = le_phy_updated,
    .le_data_len_updated = le_data_length_updated
};


static int connection_configuration_set(const struct bt_le_conn_param *conn_param,
			const struct bt_conn_le_phy_param *phy,
			const struct bt_conn_le_data_len_param *data_len)
{
	int err;
	struct bt_conn_info info = {0};

	err = bt_conn_get_info(default_conn, &info);
	if (err) {
		LOG_ERR("Failed to get connection info %d", err);
		return err;
	}

	if (info.role != BT_CONN_ROLE_MASTER) {
		LOG_ERR("'run' command shall be executed only on the master board");
	}

	err = bt_conn_le_phy_update(default_conn, phy);
	if (err) {
		LOG_ERR("PHY update failed: %d\n", err);
		return err;
	}

	LOG_INF("PHY update pending");
	/* err = k_sem_take(&throughput_sem, THROUGHPUT_CONFIG_TIMEOUT);
	if (err) {
		LOG_ERR("PHY update timeout");
		return err;
	} */

	if (info.le.data_len->tx_max_len != data_len->tx_max_len) {
		data_length_req = true;

		err = bt_conn_le_data_len_update(default_conn, data_len);
		if (err) {
			LOG_ERR("LE data length update failed: %d",
				    err);
			return err;
		}

		LOG_INF("LE Data length update pending");
		/* err = k_sem_take(&throughput_sem, THROUGHPUT_CONFIG_TIMEOUT);
		if (err) {
			LOG_ERR("LE Data Length update timeout");
			return err;
		}*/
	}

	if (info.le.interval != conn_param->interval_max) {
		err = bt_conn_le_param_update(default_conn, conn_param);
		if (err) {
			LOG_ERR("Connection parameters update failed: %d",
				    err);
			return err;
		}

		LOG_INF("Connection parameters update pending");
		/* err = k_sem_take(&throughput_sem, THROUGHPUT_CONFIG_TIMEOUT);
		if (err) {
			LOG_ERR("Connection parameters update timeout");
			return err;
		} */
	}

	return 0;
}

static void ble_params_update_work_handler(struct k_work *item)
{
     const struct bt_le_conn_param *conn_param =
            BT_LE_CONN_PARAM(INTERVAL_MIN, INTERVAL_MAX, 0, 400);

     const struct bt_conn_le_phy_param *phy = BT_CONN_LE_PHY_PARAM_2M;
     const struct bt_conn_le_data_len_param *data_len = BT_LE_DATA_LEN_PARAM_MAX;

    connection_configuration_set(conn_param, phy, data_len);
}


static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	LOG_INF("Filters matched. Address: %s connectable: %d",
		log_strdup(addr), connectable);
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	LOG_WRN("Connecting failed");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	default_conn = bt_conn_ref(conn);
}

static int nus_client_init(void)
{
	int err;
	struct bt_nus_client_init_param init = {
		.cb = {
			.received = ble_data_received,
			.sent = ble_data_sent,
		}
	};

	err = bt_nus_client_init(&nus_client, &init);
	if (err) {
		LOG_ERR("NUS Client initialization failed (err %d)", err);
		return err;
	}

	LOG_INF("NUS Client module initialized");
	return err;
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL,
		scan_connecting_error, scan_connecting);

static int scan_init(void)
{
	int err;
	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_NUS_SERVICE);
	if (err) {
		LOG_ERR("Scanning filters cannot be set (err %d)", err);
		return err;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		LOG_ERR("Filters cannot be turned on (err %d)", err);
		return err;
	}

	LOG_INF("Scan module initialized");
	return err;
}


static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", log_strdup(addr));
}


static void pairing_confirm(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	bt_conn_auth_pairing_confirm(conn);

	LOG_INF("Pairing confirmed: %s", log_strdup(addr));
}


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d", log_strdup(addr),
		bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing failed conn: %s, reason %d", log_strdup(addr),
		reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.cancel = auth_cancel,
	.pairing_confirm = pairing_confirm,
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};


uint32_t throughput_data_send(void)
{
    static char buf[THROUGH_PACKET_SIZE];
    uint32_t send_count = 0;
    int err;
    uint32_t start_time_ms, end_time_ms;
    uint32_t send_count_uplimit = THROUGHPUT_PACKETS_TO_SEND;
    uint32_t throughput_result = 0; /* kbps*/
    
    memset(buf, 's', sizeof(buf)-2);
    buf[sizeof(buf)-2] = '\r';
    buf[sizeof(buf)-1] = '\n';

    LOG_INF("Start throughput data sending, %d Kbytes", sizeof(buf)*send_count_uplimit/1000);

    start_time_ms = k_uptime_get_32();
    while(send_count < send_count_uplimit){

		//err = bt_nus_client_send(&nus_client, buf, sizeof(buf)); /* use write without response for higher throughput */
        err = bt_gatt_write_without_response(nus_client.conn, nus_client.handles.rx, buf, sizeof(buf), false);
		if (err) {
			LOG_WRN("Failed to send data over BLE connection" "(err %d)", err);
		}
        else {
            send_count++;
        }
    }

    end_time_ms = k_uptime_get_32();

    LOG_INF("Time used in ms: %d", end_time_ms - start_time_ms);
    throughput_result = ((send_count_uplimit *sizeof(buf) * 8)/(end_time_ms - start_time_ms));
    if(end_time_ms - start_time_ms > 0){
        LOG_INF("throughput %d kbps", throughput_result);
    }

    return throughput_result;
}

void ble_write_thread(void)
{
	for (;;) {
        uint32_t tempcnt = 0;
        uint64_t throughput_temp = 0;
		/* Wait indefinitely for data to be sent over bluetooth */
        /*  
		struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
						     K_FOREVER);

		if (bt_nus_send(NULL, buf->data, buf->len)) {
			LOG_WRN("Failed to send data over BLE connection");
		}

		k_free(buf);
        */
        k_sem_take(&throughput_sem, K_FOREVER); /*  wait for button press */
        while(tempcnt++  < NUMBER_THROUGHPUT_TESTS){
            
           throughput_temp += throughput_data_send();
           LOG_INF("Average throughput %u kbps", (uint32_t)(throughput_temp/tempcnt));
            k_sleep(K_MSEC(100));
        }
	}
}

void main(void)
{
	int err;

    configure_gpio();
	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_ERR("Failed to register authorization callbacks.");
		return;
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}
	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	bt_conn_cb_register(&conn_callbacks);

	int (*module_init[])(void) = {uart_init, scan_init, nus_client_init};
	for (size_t i = 0; i < ARRAY_SIZE(module_init); i++) {
		err = (*module_init[i])();
		if (err) {
			return;
		}
	}

	printk("Starting Bluetooth Central UART example\n");


	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return;
	}

	LOG_INF("Scanning successfully started");
#if 0
	for (;;) {


		/* Wait indefinitely for data to be sent over Bluetooth */
		struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
						     K_FOREVER);

		err = bt_nus_client_send(&nus_client, buf->data, buf->len);
		if (err) {
			LOG_WRN("Failed to send data over BLE connection"
				"(err %d)", err);
		}

		err = k_sem_take(&nus_write_sem, NUS_WRITE_TIMEOUT);
		if (err) {
			LOG_WRN("NUS send timeout");
		}


	}
#endif
}

K_THREAD_DEFINE(ble_write_thread_id, 4096, ble_write_thread, NULL, NULL,
		NULL, PRIORITY, 0, 0);
