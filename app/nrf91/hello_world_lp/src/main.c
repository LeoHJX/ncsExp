/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <power/power.h>
#include <hal/nrf_regulators.h>

void disable_uart()  // disable UART to measure current.
{
      NRF_UARTE0->ENABLE = 0;
      NRF_UARTE1->ENABLE = 0;
}
void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);
	disable_uart();
    nrf_regulators_system_off(NRF_REGULATORS);
    for (;;) {
    k_cpu_idle();
    }
        
}
