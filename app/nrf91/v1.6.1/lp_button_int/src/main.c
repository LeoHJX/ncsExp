/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>


#include <modem/lte_lc.h>
#include <pm/pm.h>
#include <hal/nrf_regulators.h>

#define SLEEP_TIME_MS	1

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
						     {0});

void read_print_counter(void)
{
        NRF_TIMER1_NS->TASKS_COUNT = 0x01;
        NRF_TIMER1_NS->TASKS_CAPTURE[0] = 0x01;
        printk("cc:0x%x\n", NRF_TIMER1_NS->CC[0]);
        //printk("TASKS_CAPTURE:0x%x\n", NRF_TIMER1_NS->TASKS_CAPTURE[0]);
        //printk("BITMODE:0x%x\n", NRF_TIMER1_NS->BITMODE);
        //printk("MODE:0x%x\n", NRF_TIMER1_NS->MODE);
        //printk("TASKS_START:0x%x\n", NRF_TIMER1_NS->TASKS_START);
        //NRF_TIMER1_NS->TASKS_COUNT = 0x00;
}

void trigger_counter(void)
{
    NRF_TIMER1_NS->TASKS_COUNT = 0x01;
}
void start_timer_counter(void)
{
    NRF_TIMER1_NS->TASKS_CLEAR = 0x01;
    NRF_TIMER1_NS->BITMODE = 0x03;
    NRF_TIMER1_NS->MODE = 0x01;
    NRF_TIMER1_NS->PRESCALER  = 0x0;
    NRF_TIMER1_NS->TASKS_START = 0x01;
    NRF_TIMER1_NS->TASKS_CLEAR = 0x01;
    printk("cc:0x%x\n", NRF_TIMER1_NS->CC[0]);
    printk("TASKS_CAPTURE:0x%x\n", NRF_TIMER1_NS->TASKS_CAPTURE[0]);
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{

#if 0
    int ret;

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_DISABLE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return;
	}
#endif
	//printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
    /* If we have an LED, match its state to the button's. */
    int val = gpio_pin_get(button.port, button.pin);
    gpio_pin_set(led.port, led.pin, val);
    read_print_counter();

    /* */
}


void disable_uart() // disable UART to measure current.
{
	NRF_UARTE0->ENABLE = 0;
	NRF_UARTE1->ENABLE = 0;
}

void main(void)
{
	int ret;
    disable_uart();
    lte_lc_power_off();
    start_timer_counter();
    
	if (!device_is_ready(button.port)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return;
	}
    
	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	if (led.port && !device_is_ready(led.port)) {
		printk("Error %d: LED device %s is not ready; ignoring it\n",
		       ret, led.port->name);
		led.port = NULL;
	}
	if (led.port) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure LED device %s pin %d\n",
			       ret, led.port->name, led.pin);
			led.port = NULL;
		} else {
			printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}

	printk("Press the button\n");
	if (led.port) {
		while (1) {
			/* If we have an LED, match its state to the button's. 
			int val = gpio_pin_get(button.port, button.pin);

			if (val >= 0) {
				gpio_pin_set(led.port, led.pin, val);
			}
			k_msleep(SLEEP_TIME_MS);*/
            //k_cpu_idle();
            trigger_counter();  // trigger every 10 seconds. 
#if 0
            ret = gpio_pin_interrupt_configure_dt(&button,
                                  GPIO_INT_EDGE_TO_ACTIVE);
            if (ret != 0) {
                printk("Error %d: failed to configure interrupt on %s pin %d\n",
                    ret, button.port->name, button.pin);
                return;
            }
#endif
            k_msleep(10000);

		}
	}
}
