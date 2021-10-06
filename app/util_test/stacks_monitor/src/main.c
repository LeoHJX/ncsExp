/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

//#include <debug/stack.h>

/** for thread example */

//#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
//#include <sys/printk.h>
#include <sys/__assert.h>
#include <string.h>


/* size of stack area used by each thread */
#define STACKSIZE 512

/* scheduling priority used by each thread */
#define PRIORITY 7

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED1_NODE, okay)
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

struct printk_data_t {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	uint32_t led;
	uint32_t cnt;
};

K_FIFO_DEFINE(printk_fifo);

struct led {
	struct gpio_dt_spec spec;
	const char *gpio_pin_name;
};

static const struct led led0 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0}),
	.gpio_pin_name = DT_PROP_OR(LED0_NODE, label, ""),
};

static const struct led led1 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED1_NODE, gpios, {0}),
	.gpio_pin_name = DT_PROP_OR(LED1_NODE, label, ""),
};

void blink(const struct led *led, uint32_t sleep_ms, uint32_t id)
{
	const struct gpio_dt_spec *spec = &led->spec;
	int cnt = 0;
	int ret;

	if (!device_is_ready(spec->port)) {
		printk("Error: %s device is not ready\n", spec->port->name);
		return;
	}

	ret = gpio_pin_configure_dt(spec, GPIO_OUTPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure pin %d (LED '%s')\n",
			ret, spec->pin, led->gpio_pin_name);
		return;
	}

	while (1) {
		gpio_pin_set(spec->port, spec->pin, cnt % 2);

		struct printk_data_t tx_data = { .led = id, .cnt = cnt };

		size_t size = sizeof(struct printk_data_t);
		char *mem_ptr = k_malloc(size);
		__ASSERT_NO_MSG(mem_ptr != 0);

		memcpy(mem_ptr, &tx_data, size);

		k_fifo_put(&printk_fifo, mem_ptr);

		k_msleep(sleep_ms);
		cnt++;
	}
}

void blink0(void)
{
	blink(&led0, 100, 0);
}

void blink1(void)
{
	blink(&led1, 1000, 1);
}

void uart_out(void)
{
	while (1) {
		struct printk_data_t *rx_data = k_fifo_get(&printk_fifo,
							   K_FOREVER);
		/* printk("Toggled led%d; counter=%d\n",
		       rx_data->led, rx_data->cnt);  */  /*  comment out for now, avoid mess with the termina output */
		k_free(rx_data);
	}
}

K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,
		PRIORITY, 0, 0);
K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL,
		PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_out_id, STACKSIZE, uart_out, NULL, NULL, NULL,
		PRIORITY, 0, 0);


/** end for thread example */


#if defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_STACK_INFO) && \
	defined(CONFIG_THREAD_MONITOR)


extern K_KERNEL_STACK_ARRAY_DEFINE(z_interrupt_stacks, CONFIG_MP_NUM_CPUS,
				   CONFIG_ISR_STACK_SIZE);

static void shell_stack_dump(const struct k_thread *thread, void *user_data)
{
	unsigned int pcnt;
	size_t unused;
	size_t size = thread->stack_info.size;
	const char *tname;
	int ret;

    ARG_UNUSED(user_data);
	ret = k_thread_stack_space_get(thread, &unused);
	if (ret) {
        printk("Unable to determine unused stack size (%d)\n",
			    ret);
		return;
	}

	tname = k_thread_name_get((struct k_thread *)thread);

	/* Calculate the real size reserved for the stack */
	pcnt = ((size - unused) * 100U) / size;

	printk("%p %-10s (real size %u):\tunused %u\tusage %u / %u (%u %%)\n",
		      thread,
		      tname ? tname : "NA",
		      size, unused, size - unused, size, pcnt);
}
void display_stacks(void)
{
	uint8_t *buf;
	size_t size, unused;

	k_thread_foreach(shell_stack_dump, NULL);

	/* Placeholder logic for interrupt stack until we have better
	 * kernel support, including dumping arch-specific exception-related
	 * stack buffers.
	 */
	for (int i = 0; i < CONFIG_MP_NUM_CPUS; i++) {
		buf = Z_KERNEL_STACK_BUFFER(z_interrupt_stacks[i]);
		size = K_KERNEL_STACK_SIZEOF(z_interrupt_stacks[i]);

		unused = 0;
		for (size_t i = 0; i < size; i++) {
			if (buf[i] == 0xAAU) {
				unused++;
			} else {
				break;
			}
		}

		printk("%p IRQ %02d     (real size %zu):\tunused %zu\tusage %zu / %zu (%zu %%)\n",
			      &z_interrupt_stacks[i], i, size, unused,
			      size - unused, size,
			      ((size - unused) * 100U) / size);
	}

	/* return 0; */
}

#endif /*   #if defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_STACK_INFO) && \
	defined(CONFIG_THREAD_MONITOR)  */

void main(void)
{
	printk("#######Hello World! %s ########\n", CONFIG_BOARD);
    
    while(1)
    {
        printk("#####################################\n");
        display_stacks();
        k_sleep(K_MSEC(1000));
    }
}
