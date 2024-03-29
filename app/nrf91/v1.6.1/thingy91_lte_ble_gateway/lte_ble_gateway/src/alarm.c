/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <string.h>
#include <stdbool.h>
#include <sys/printk.h>
#include <net/nrf_cloud.h>

#include "alarm.h"

#include "aggregator.h"

static bool alarm_pending;

extern void sensor_data_send(struct nrf_cloud_sensor_data *data);

char *orientation_strings[] = {"LEFT", "NORMAL", "RIGHT", "UPSIDE_DOWN"};
char *button_strings[] = {"Release", "Press"};

void alarm(void)
{
	alarm_pending = true;
}

void send_aggregated_data(void)
{
	static uint8_t gps_data_buffer[GPS_NMEA_SENTENCE_MAX_LENGTH];

	static struct nrf_cloud_sensor_data gps_cloud_data = {
		.type = NRF_CLOUD_SENSOR_GPS,
		.data.ptr = gps_data_buffer,
	};

	static struct nrf_cloud_sensor_data flip_cloud_data = {
		.type = NRF_CLOUD_SENSOR_FLIP,
	};
	static struct nrf_cloud_sensor_data button_cloud_data = {
		.type = NRF_CLOUD_SENSOR_BUTTON,
	};
	struct sensor_data aggregator_data;

	if (!alarm_pending) {
		return;
	}

	alarm_pending = false;

	printk("Alarm triggered !\n");
	while (1) {
		if (aggregator_get(&aggregator_data) == -ENODATA) {
			break;
		}
		switch (aggregator_data.type) {
		case THINGY_ORIENTATION:
			printk("%d] Sending FLIP data.\n",
			       aggregator_element_count_get());
			if (aggregator_data.length != 1 ||
				aggregator_data.data[0] >=
				ARRAY_SIZE(orientation_strings)) {
				printk("Unexpected FLIP data format, dropping\n");
				continue;
			}
			flip_cloud_data.data.ptr =
				orientation_strings[aggregator_data.data[0]];
			flip_cloud_data.data.len = strlen(
				orientation_strings[aggregator_data.data[0]]) - 1;
			sensor_data_send(&flip_cloud_data);
			break;

		case GPS_POSITION:
			printk("%d] Sending GPS data.\n",
			       aggregator_element_count_get());
			gps_cloud_data.data.ptr = &aggregator_data.data[4];
			gps_cloud_data.data.len = aggregator_data.length;
			gps_cloud_data.tag =
			    *((uint32_t *)&aggregator_data.data[0]);
			sensor_data_send(&gps_cloud_data);
			break;
		case THINGY_BUTTON:
			printk("%d] Sending button data.\n",
			       aggregator_element_count_get());
			if (aggregator_data.length != 1 ||
				aggregator_data.data[0] >=
				ARRAY_SIZE(button_strings)) {
				printk("Unexpected button data format, dropping\n");
				continue;
			}
			button_cloud_data.data.ptr =
				button_strings[aggregator_data.data[0]];
			button_cloud_data.data.len = strlen(
				button_strings[aggregator_data.data[0]]) - 1;
			sensor_data_send(&button_cloud_data);
	
			break;

		default:
			printk("Unsupported data type from aggregator: %d.\n",
			       aggregator_data.type);
			continue;
		}
	}
}
