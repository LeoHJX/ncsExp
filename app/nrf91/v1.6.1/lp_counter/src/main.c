/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>


#include <modem/lte_lc.h>
#include <pm/pm.h>
#include <hal/nrf_regulators.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

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


void disable_uart() // disable UART to measure current.
{
	NRF_UARTE0->ENABLE = 0;
	NRF_UARTE1->ENABLE = 0;
}


void main(void)
{
	const struct device *dev;
	bool led_is_on = true;
	int ret;

    disable_uart();
    lte_lc_power_off();
	dev = device_get_binding(LED0);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		return;
	}
    uint32_t cnt = 0;

    

	while (1) {
		gpio_pin_set(dev, PIN, (int)led_is_on);
        NRF_TIMER1_NS->TASKS_COUNT = 0x01;
        NRF_TIMER1_NS->TASKS_CAPTURE[0] = 0x01;
        printk("cc:0x%x\n", NRF_TIMER1_NS->CC[0]);
        //printk("TASKS_CAPTURE:0x%x\n", NRF_TIMER1_NS->TASKS_CAPTURE[0]);
        //printk("BITMODE:0x%x\n", NRF_TIMER1_NS->BITMODE);
        //printk("MODE:0x%x\n", NRF_TIMER1_NS->MODE);
        //printk("TASKS_START:0x%x\n", NRF_TIMER1_NS->TASKS_START);
        //NRF_TIMER1_NS->TASKS_COUNT = 0x00;
		led_is_on = !led_is_on;
        cnt++;
        printk("toggle LED: %d\n\r", cnt);
		k_msleep(SLEEP_TIME_MS * 10);
	}
}
