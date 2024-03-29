/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <bluetooth/conn.h>

#include "peer.h"
#include "pwm_led.h"
#include "service.h"
#include <dk_buttons_and_leds.h>
#define PEER_MAX                8   /* Maximum number of tracked peer devices. */

#define STACKSIZE               1024
#define PEER_THREAD_PRIORITY    K_LOWEST_APPLICATION_THREAD_PRIO

#define PEER_TIMEOUT_STEP_MS    500
#define PEER_TIMEOUT_INIT_MS    10000

#define DISTANCE_MAX_LED         70   /* from 0 to DISTANCE_MAX_LED [decimeter] -
				      * the distance range which is indicated by the PWM LED
				      */
#define DISTANCE_R_MAX_LED        10   /* led green range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */
#define DISTANCE_G_MAX_LED        20   /* led Red range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */
#define DISTANCE_B_MAX_LED        30   /* led Red range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */					 
#define DISTANCE_RG_MAX_LED       40   /* led Red range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */
#define DISTANCE_RB_MAX_LED       50   /* led Red range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */
#define DISTANCE_GB_MAX_LED       60   /* led Red range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */					 
#define DISTANCE_RGB_MAX_LED      70   /* led RGB range [decimeter] -
				      * the distance range which is indicated by PWM LED
				      */
#define LED_R DK_LED2
#define LED_G DK_LED3
#define LED_B DK_LED4

#define TOTAL_NUM_FILTER 6
#define EX_NUM_FILTER 2  /*  EX_NUM_FILTER *  2 must < TOTAL_NUM_FILTER //2 upper and 2 lower, total 4 in average */

#define DEFAULT_RANGING_MODE    DM_RANGING_MODE_MCPD

static void timeout_handler(struct k_timer *timer_id);
static K_TIMER_DEFINE(timer, timeout_handler, NULL);

struct peer_entry {
	sys_snode_t node;
	bt_addr_le_t bt_addr;
	struct dm_result result;
	uint16_t timeout_ms;
};

static struct peer_entry *closest_peer;
static uint32_t access_address;
static enum dm_ranging_mode ranging_mode = DEFAULT_RANGING_MODE;

K_MSGQ_DEFINE(result_msgq, sizeof(struct dm_result), 16, 4);

static K_HEAP_DEFINE(peer_heap, PEER_MAX * sizeof(struct peer_entry));
static sys_slist_t peer_list = SYS_SLIST_STATIC_INIT(&peer_list);

static K_MUTEX_DEFINE(list_mtx);

static void list_lock(void)
{
	k_mutex_lock(&list_mtx, K_FOREVER);
}

static void list_unlock(void)
{
	k_mutex_unlock(&list_mtx);
}

static struct peer_entry *mcpd_min_peer_result(struct peer_entry *a, struct peer_entry *b)
{
	if (!a && !b) {
		return NULL;
	} else if (!a && b) {
		return b->result.ranging_mode == DM_RANGING_MODE_MCPD ? b : NULL;
	} else if (a && !b) {
		return a->result.ranging_mode == DM_RANGING_MODE_MCPD ? a : NULL;
	} else if (a->result.ranging_mode == DM_RANGING_MODE_RTT &&
		   b->result.ranging_mode == DM_RANGING_MODE_RTT) {
		return NULL;
	} else if (a->result.ranging_mode == DM_RANGING_MODE_RTT &&
		   b->result.ranging_mode == DM_RANGING_MODE_MCPD) {
		return b;
	} else if (a->result.ranging_mode == DM_RANGING_MODE_MCPD &&
		   b->result.ranging_mode == DM_RANGING_MODE_RTT){
		return a;
	}

	if (a->result.dist_estimates.mcpd.best > b->result.dist_estimates.mcpd.best) {
		return a;
	}
	return b;
}

static struct peer_entry *peer_find_closest(void)
{
	struct peer_entry *closest_peer, *item;
	sys_snode_t *node, *tmp;

	closest_peer = SYS_SLIST_PEEK_HEAD_CONTAINER(&peer_list, closest_peer, node);
	if (!closest_peer) {
		return NULL;
	}

	SYS_SLIST_FOR_EACH_NODE_SAFE(&peer_list, node, tmp) {
		item = CONTAINER_OF(node, struct peer_entry, node);
		closest_peer = mcpd_min_peer_result(closest_peer, item);
	}

	return closest_peer;
}

struct peer_entry *peer_find(const bt_addr_le_t *peer)
{
	if (!peer) {
		return NULL;
	}

	sys_snode_t *node, *tmp;
	struct peer_entry *item;

	SYS_SLIST_FOR_EACH_NODE_SAFE(&peer_list, node, tmp) {
		item = CONTAINER_OF(node, struct peer_entry, node);
		if (bt_addr_le_cmp(&item->bt_addr, peer) == 0) {
			return item;
		}
	}

	return NULL;
}

void swap(float* xp, float* yp)
{
    float temp = *xp;
    *xp = *yp;
    *yp = temp;
}
 
// Function to perform Selection Sort
void selectionSort(float arr[], int n)
{
    int i, j, min_idx;
 
    // One by one move boundary of unsorted subarray
    for (i = 0; i < n - 1; i++) {
 
        // Find the minimum element in unsorted array
        min_idx = i;
        for (j = i + 1; j < n; j++)
            if (arr[j] < arr[min_idx])
                min_idx = j;
 
        // Swap the found minimum element
        // with the first element
        swap(&arr[min_idx], &arr[i]);
    }
}
 
float average_cacu(float *ptr)
{
	uint8_t i = EX_NUM_FILTER;
	float total = 0;
	while (i< (TOTAL_NUM_FILTER - EX_NUM_FILTER))
	{
		total += ptr[i];
		i++;
	}
	return total/(TOTAL_NUM_FILTER - (EX_NUM_FILTER * 2));
}


float filt_buf[TOTAL_NUM_FILTER];
uint8_t collect_cnt = 0;

uint8_t data_collect(float *buf, float input)
{
	buf[collect_cnt] = input;
	collect_cnt++;
	return collect_cnt;
}
static uint8_t filter_result(float *rreturn, float res_input)
{
	uint8_t rstatus = 0;

	if(data_collect(filt_buf, res_input) == TOTAL_NUM_FILTER)
	{
		// sort now, 
		selectionSort(filt_buf, TOTAL_NUM_FILTER);
		// average now. and place result to return pointer.
		*rreturn = average_cacu(filt_buf);
		collect_cnt = 0; // now start collect again.
		rstatus = 1;
		return rstatus; // indicating this result is good filtered good result. 
	}
	return rstatus;
}

static void ble_notification(const struct peer_entry *peer)
{
	service_distance_measurement_update(&peer->bt_addr, &peer->result);
}

static void led_notification(const struct peer_entry *peer)
{
	if (!peer) {
		dk_set_led_off(LED_R);
		dk_set_led_off(LED_G);
		dk_set_led_off(LED_B);
		return;
	}

	float res;
	float rreturn;

	if (peer->result.ranging_mode == DM_RANGING_MODE_RTT) {
		res = peer->result.dist_estimates.rtt.rtt;
	} else {
		res = peer->result.dist_estimates.mcpd.best;
	}

	res = (res < 0 ? 0 : res);
	if(filter_result(&rreturn, res))
	{
		res =  rreturn;
	}
	else 
	{
		return;
	}
	res = res *  10;
	if (res > DISTANCE_MAX_LED) {
		dk_set_led_off(LED_R);
		dk_set_led_off(LED_G);
		dk_set_led_off(LED_B);
	} else {
		
		if(res <= DISTANCE_R_MAX_LED) //R 0-1m 
		{
			dk_set_led_off(LED_R);
			dk_set_led_on(LED_G);
			dk_set_led_off(LED_B);	
		}
		/* 
		else if ((res > DISTANCE_R_MAX_LED) && (res <= DISTANCE_G_MAX_LED)) //G 1m - 2m
		{
			dk_set_led_off(LED_R);
			dk_set_led_on(LED_G);
			dk_set_led_off(LED_B);			

		}
		else if((res > DISTANCE_G_MAX_LED) && (res <= DISTANCE_B_MAX_LED))   // B 2m - 3m
		{
			dk_set_led_off(LED_R);
			dk_set_led_off(LED_G);
			dk_set_led_on(LED_B);			
	
		}
		else if((res > DISTANCE_B_MAX_LED) && (res <= DISTANCE_RG_MAX_LED))   // RG 3m - 4m
		{
			dk_set_led_on(LED_R);
			dk_set_led_on(LED_G);
			dk_set_led_off(LED_B);			
		}	
		else if((res > DISTANCE_RG_MAX_LED) && (res <= DISTANCE_RB_MAX_LED))   // RB 4m - 5m
		{
			dk_set_led_on(LED_R);
			dk_set_led_off(LED_G);
			dk_set_led_on(LED_B);			
		}	
		else if((res > DISTANCE_RB_MAX_LED) && (res <= DISTANCE_GB_MAX_LED))   // GB 5m - 6m
		{
			dk_set_led_off(LED_R);
			dk_set_led_on(LED_G);
			dk_set_led_on(LED_B);			
		}
		else if((res > DISTANCE_GB_MAX_LED) && (res <= DISTANCE_RGB_MAX_LED))   // RGB 6m - 7m
		{
			dk_set_led_on(LED_R);
			dk_set_led_on(LED_G);
			dk_set_led_on(LED_B);			
		}*/
		else  // above 1m.
		{
			dk_set_led_on(LED_R);
			dk_set_led_off(LED_G);
			dk_set_led_off(LED_B);	
		}
	}
}

static void print_result(struct dm_result *result)
{
	float rreturn = 0;
	if (!result) {
		return;
	}

	const char *quality[DM_QUALITY_NONE + 1] = {"ok", "poor", "do not use", "crc fail", "none"};
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&result->bt_addr, addr, sizeof(addr));

	printk("\nMeasurement result:\n");
	printk("\tAddr: %s\n", addr);
	printk("\tQuality: %s\n", quality[result->quality]);

	printk("\tDistance estimates: ");
	if (result->ranging_mode == DM_RANGING_MODE_RTT) {
		printk("rtt: rtt=%.2f\n", result->dist_estimates.rtt.rtt);
		if(filter_result(&rreturn, result->dist_estimates.rtt.rtt))
		{
			printk("\n\rrtt Average output =%.2f\n\r", rreturn);
		}		
	} else {
		printk("mcpd: ifft=%.2f phase_slope=%.2f rssi_openspace=%.2f best=%.2f\n",
			result->dist_estimates.mcpd.ifft,
			result->dist_estimates.mcpd.phase_slope,
			result->dist_estimates.mcpd.rssi_openspace,
			result->dist_estimates.mcpd.best);
		if(filter_result(&rreturn, result->dist_estimates.mcpd.best))
		{
			printk("\n\rmcpd Average output =%.2f\n\r", rreturn);
		}			
	}

}

static void timeout_handler(struct k_timer *timer_id)
{
	sys_snode_t *node, *tmp;
	struct peer_entry *item;

	SYS_SLIST_FOR_EACH_NODE_SAFE(&peer_list, node, tmp) {
		item = CONTAINER_OF(node, struct peer_entry, node);
		if (item->timeout_ms > PEER_TIMEOUT_STEP_MS) {
			item->timeout_ms -= PEER_TIMEOUT_STEP_MS;
		} else {
			sys_slist_remove(&peer_list, NULL, node);
			k_heap_free(&peer_heap, item);
			closest_peer = peer_find_closest();

			led_notification(closest_peer);
		}
	}
}

static void peer_thread(void)
{
	struct dm_result result;

	while (1) {
		if (k_msgq_get(&result_msgq, &result, K_FOREVER) == 0) {
			struct peer_entry *peer;

			peer = peer_find(&result.bt_addr);
			if (!peer) {
				continue;
			}

			memcpy(&peer->result, &result, sizeof(peer->result));
			peer->timeout_ms = PEER_TIMEOUT_INIT_MS;
			print_result(&peer->result);

			closest_peer = mcpd_min_peer_result(closest_peer, peer);

			led_notification(closest_peer);
			ble_notification(peer);
		}
	}
}

void peer_ranging_mode_set(enum dm_ranging_mode mode)
{
	ranging_mode = mode;
}

enum dm_ranging_mode peer_ranging_mode_get(void)
{
	return ranging_mode;
}

int peer_access_address_prepare(void)
{
	bt_addr_le_t addr = {0};
	size_t count = 1;

	bt_id_get(&addr, &count);

	access_address = addr.a.val[0];
	access_address |= addr.a.val[1] << 8;
	access_address |= addr.a.val[2] << 16;
	access_address |= addr.a.val[3] << 24;

	if (access_address == 0) {
		return -EFAULT;
	}

	return 0;
}

uint32_t peer_access_address_get(void)
{
	return access_address;
}

bool peer_supported_test(const bt_addr_le_t *peer)
{
	sys_snode_t *node, *tmp;
	struct peer_entry *item;

	SYS_SLIST_FOR_EACH_NODE_SAFE(&peer_list, node, tmp) {
		item = CONTAINER_OF(node, struct peer_entry, node);
		if (bt_addr_le_cmp(&item->bt_addr, peer) == 0) {
			return true;
		}
	}

	return false;
}

int peer_supported_add(const bt_addr_le_t *peer)
{
	struct peer_entry *item;

	if (peer_supported_test(peer)) {
		return 0;
	}

	item = k_heap_alloc(&peer_heap, sizeof(struct peer_entry), K_NO_WAIT);
	if (!item) {
		return -ENOMEM;
	}

	item->timeout_ms = PEER_TIMEOUT_INIT_MS;
	bt_addr_le_copy(&item->bt_addr, peer);
	list_lock();
	sys_slist_append(&peer_list, &item->node);
	list_unlock();

	return 0;
}

void peer_update(struct dm_result *result)
{
	k_msgq_put(&result_msgq, result, K_NO_WAIT);
}

int peer_init(void)
{
	k_timer_start(&timer, K_NO_WAIT, K_MSEC(PEER_TIMEOUT_STEP_MS));

	return 0;
}

K_THREAD_DEFINE(peer_consumer_thred_id, STACKSIZE, peer_thread,
		NULL, NULL, NULL, PEER_THREAD_PRIORITY, 0, 0);
