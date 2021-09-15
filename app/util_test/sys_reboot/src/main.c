/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <sys/reboot.h>

void main(void)
{
	printk("Hello World! %s\n", "Test board with MCU Boot on second slot");
	k_sleep(K_MSEC(3000));
	sys_reboot(0);  // warm.
	//sys_reboot(1); // cold. 
}

