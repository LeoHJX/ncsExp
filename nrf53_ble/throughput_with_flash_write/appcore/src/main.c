#include <zephyr.h>
#include <drivers/ipm.h>
#include <sys/printk.h>
#include <device.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <init.h>

#include <ipc/rpmsg_service.h>
#include <drivers/flash.h>
#include <device.h>
#include <storage/stream_flash.h>
#include "nrfx_clock.h"

#define APP_CORE_128MHZ
#define FLASH_TEST_OFFSET 0x40000
#define APP_TASK_STACK_SIZE (1024)
#define FLASH_PAGE_SIZE 4096
#define PAGES 150

static int ep_id;
struct rpmsg_endpoint my_ept;
struct rpmsg_endpoint *ep = &my_ept;

K_THREAD_STACK_DEFINE(thread_stack, APP_TASK_STACK_SIZE);
static struct k_thread thread_data;
static uint8_t received_data[250];
static uint16_t received_data_len;

struct stream_flash_ctx stream;
const struct device *flash_dev;
uint8_t stream_flash_buf[4096];

static K_SEM_DEFINE(data_rx_sem, 0, 1);


void flash_test_area_erase()
{
	flash_write_protection_set(flash_dev, false);
	if (flash_erase(flash_dev, FLASH_TEST_OFFSET, FLASH_PAGE_SIZE * PAGES) != 0) {
		printk("Flash erase failed!\n");
	} else {
		printk("Flash erase succeeded!\n");
	}
}

void flash_driver_init()
{

	flash_dev =
	device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);

	if (!flash_dev) {
		printk("Nordic nRF5 flash driver was not found!\n");
		return;
	}

	flash_test_area_erase();

	if (stream_flash_init(&stream, flash_dev, stream_flash_buf, sizeof(stream_flash_buf), FLASH_TEST_OFFSET, 0, NULL)){
		printk("stream_flash_init failed\n");
	} else {
		printk("stream flash init succeeded\n");
	}
	

}


void data_handler(uint8_t *data, size_t len)
{
	memcpy(received_data, data, len);
	received_data_len = len;
}


int endpoint_cb(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	data_handler(data, len);
	k_sem_give(&data_rx_sem);

	return RPMSG_SUCCESS;
}


static int send_message(unsigned int message)
{
	return rpmsg_service_send(ep_id, &message, sizeof(message));
}


static int network_gpio_allow(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* When the use of the low frequency crystal oscillator (LFXO) is
	 * enabled, do not modify the configuration of the pins P0.00 (XL1)
	 * and P0.01 (XL2), as they need to stay configured with the value
	 * Peripheral.
	 */
	uint32_t start_pin = (IS_ENABLED(CONFIG_SOC_ENABLE_LFXO) ? 2 : 0);

	/* Allow the network core to use all GPIOs. */
	for (uint32_t i = start_pin; i < P0_PIN_NUM; i++) {
		NRF_P0_S->PIN_CNF[i] = (GPIO_PIN_CNF_MCUSEL_NetworkMCU <<
					GPIO_PIN_CNF_MCUSEL_Pos);
	}

	for (uint32_t i = 0; i < P1_PIN_NUM; i++) {
		NRF_P1_S->PIN_CNF[i] = (GPIO_PIN_CNF_MCUSEL_NetworkMCU <<
					GPIO_PIN_CNF_MCUSEL_Pos);
	}

	return 0;
}

void app_task(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	unsigned int message = 0;
	int err;
	static uint32_t i = 0;

	/* Since we are using name service, we need to wait for a response
	 * from NS setup and than we need to process it
	 */
	while (!rpmsg_service_endpoint_is_bound(ep_id)) {
		k_sleep(K_MSEC(1));
	}
	
	send_message(message);
	while (1) {
		k_sem_take(&data_rx_sem, K_FOREVER);

		if(received_data_len == 200){
			err =stream_flash_buffered_write(&stream, received_data, received_data_len, true);
			if (err) {
				printk("stream_flash_buffered_write failed, err = %d\n", err);
			}
			printk("\nLast packet received\n");
			break;
		}else{
			err =stream_flash_buffered_write(&stream, received_data, received_data_len, false);
			if (err) {
				printk("stream_flash_buffered_write failed, err = %d\n", err);
			}
			printk("=");
		}
		i++;
		if (i % 80 == 0) {
			printk("\n");
		}
		send_message(message);
		

	}
	printk("Flash write and throughput demo ended.\n");
	printk("Please reboot device for next test\n");

}

#ifdef APP_CORE_128MHZ
static int core_app_config(void)
{
	int ret;
	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);

	/* Enable YOPAN-53 for lower power consumption on nRF5340 */
	*((volatile uint32_t *)0x50004728ul) = 0x1;

	return 0;
}
#endif

void main(void)
{
	printk("Application core start\n");
	printk("Please wait for test flash area be erased\n");
#ifdef APP_CORE_128MHZ	
	core_app_config();
#endif	
	flash_driver_init();
	printk("Starting application thread!\n");
	k_thread_create(&thread_data, thread_stack, APP_TASK_STACK_SIZE,
			(k_thread_entry_t)app_task,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
}

/* Make sure we register endpoint before RPMsg Service is initialized. */
int register_endpoint(const struct device *arg)
{
	int status;

	status = rpmsg_service_register_endpoint("demo", endpoint_cb);

	if (status < 0) {
		printk("rpmsg_create_ept failed %d\n", status);
		return status;
	}

	ep_id = status;

	return 0;
}

SYS_INIT(register_endpoint, POST_KERNEL, CONFIG_RPMSG_SERVICE_EP_REG_PRIORITY);
SYS_INIT(network_gpio_allow, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
