/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

/* Jut a example: this static data will be placed in the specified flash, after the firmware info.   */
Z_GENERIC_SECTION(.custom_flash_vars) __attribute__((used))
const uint8_t g_var[] = {0x55, 0xaa, 0x55, 0xaa};

void main(void)
{

	printk("Hello World! %s\n", CONFIG_BOARD);
}
