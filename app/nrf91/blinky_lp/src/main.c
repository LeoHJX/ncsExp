/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>


#include <power/power.h>
#include <hal/nrf_regulators.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   3000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#define LED0	DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED0	""
#define PIN	0
#define FLAGS	0
#endif
void disable_uart()  // disable UART to measure current.
{
      NRF_UARTE0->ENABLE = 0;
      NRF_UARTE1->ENABLE = 0;
}
void main(void)
{
	const struct device *dev;
	bool led_is_on = true;
	int ret;
	int blinkCnt = 0;

	dev = device_get_binding(LED0);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		return;
	}
	disable_uart();
	while (1) {
		blinkCnt++;
		gpio_pin_set(dev, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);
		
#if 1   /* 1: make sure App core always in idle */		
		while(1){
			k_cpu_idle();  // single thread, call this once will be ok. 
		}
#endif

#if 0  /* trun off   */
	if(blinkCnt > 2){
                //nrf_regulators_dcdcen_set(NRF_REGULATORS,0);
		nrf_regulators_system_off(NRF_REGULATORS);
	}
	
#endif
	}
}
