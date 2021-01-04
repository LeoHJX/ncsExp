/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <modem/lte_lc.h>
#include <power/power.h>
#include <hal/nrf_regulators.h>

void disable_uart() // disable UART to measure current.
{
	NRF_UARTE0->ENABLE = 0;
	NRF_UARTE1->ENABLE = 0;
}
void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	lte_lc_power_off();
	k_sleep(K_MSEC(1200));
	disable_uart();
	k_sleep(K_MSEC(100));
#if 0 /*   App core off option */
	nrf_regulators_system_off(NRF_REGULATORS_NS);
#else /* idle otherwise */

	/* then app core idle   */
	k_cpu_idle();
	for (;;) {
		k_cpu_idle();
	}
#endif
}
