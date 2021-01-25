/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <zephyr/types.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/scan.h>
#include <bluetooth/uuid.h>

#include <bluetooth/hci_vs.h>
#include <devicetree.h>
#include <drivers/gpio.h>

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS DT_GPIO_FLAGS(LED0_NODE, gpios)

//static bt_addr_t addr =   {0x28, 0x80, 0x53, 0x67, 0xED, 0xE3};

static const uint8_t dev_name[] = "Test beacon";

static uint8_t count = 0;

static void scan_filter_match(struct bt_scan_device_info *device_info,
    struct bt_scan_filter_match *filter_match,
    bool connectable)
{
    int8_t temp[255];
    uint8_t data_offset = 0;
    printk("Scanned Test Beacon count = %d\n", count++);
    /* possible to add futher filtering here.   */
    printk("adv data len: %d\n", device_info->adv_data->len);
    printk("recv adrs type: %d\n", device_info->recv_info->addr->type);
    printk("recv adrs:%02X%02X%02X%02X%02X%02X\n",
        device_info->recv_info->addr->a.val[0],
        device_info->recv_info->addr->a.val[1],
        device_info->recv_info->addr->a.val[2],
        device_info->recv_info->addr->a.val[3],
        device_info->recv_info->addr->a.val[4],
        device_info->recv_info->addr->a.val[5]);
    printk("RSSI: %d\n", device_info->recv_info->rssi);
    printk("recv data0 type : %02x\n", device_info->adv_data->data[1]);
    printk("recv data0 len :  %d\n", device_info->adv_data->data[0]);
    memset(temp, 0, 255);
    memcpy(temp, &(device_info->adv_data->data[2]), device_info->adv_data->data[0] - 1);
    printk("recv data0 data : %s\n", temp);

    data_offset = device_info->adv_data->data[0] + 1; /* +2: offset the lengh byte */

    printk("recv data1 type : %02x\n", device_info->adv_data->data[data_offset + 1]);
    printk("recv data1 len :  %d\n", device_info->adv_data->data[data_offset]);
    printk("recv data1 id :  %02X%02X\n", device_info->adv_data->data[data_offset + 3], device_info->adv_data->data[data_offset + 2]);
    memset(temp, 0, 255);
    memcpy(temp, &(device_info->adv_data->data[data_offset + 4]), device_info->adv_data->data[data_offset] - 3); /* 3 bytes including length, and manufacture ID 2 bytes */
    printk("recv data1 data : %s\n", temp);
    /* device_info->conn_param;
    
    filter_match->addr;
    filter_match->appearance;
    filter_match->manufacturer_data;
    filter_match->name;
    filter_match->short_name;
    filter_match->uuid;
    connectable;  
    */
}

void local_mac_reg_printout(void)
{
    uint32_t reg0, reg1;

    reg0 = NRF_FICR->DEVICEADDR[0];
    reg1 = NRF_FICR->DEVICEADDR[1];

    /* printk("reg0: 0x%X, reg1: 0x%X\n\r", reg0, reg1); // debug reg reading */
    /* last one set the highest 2 bits to 1 accroding to the spec.   */

    printk("local Mac: %02X:%02X:%02X:%02X:%02X:%02X \n\r", reg0 & 0xff, (reg0 >> 8) & 0xff, (reg0 >> 16) & 0xff, (reg0 >> 24) & 0xff, (reg1 >> 0) & 0xff, (0xC0) | ((reg1 >> 8) & 0xff));
}

void local_mac_idget_printout(void)
{

    bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
    size_t a_size = CONFIG_BT_ID_MAX;

    bt_id_get(addrs, &a_size);

    for (uint8_t i = 0; i < a_size; i++)
    {
        printk("adrs %d -> ", i);
        for (uint8_t idx = 0; idx < 6; idx++)
        {
            printk("%02X ", addrs[i].a.val[idx]);
        }
        printk("\n\r");
    }
}
void local_mac_printout(void)
{
    struct bt_hci_vs_static_addr addrs[CONFIG_BT_ID_MAX];

    bt_read_static_addr(addrs, CONFIG_BT_ID_MAX);

    if (CONFIG_BT_ID_MAX)
    {
        for (uint8_t i = 0; i < CONFIG_BT_ID_MAX; i++)
        {
            printk("adrs %d -> ", i);
            for (uint8_t idx = 0; idx < 6; idx++)
            {
                printk("%02X ", addrs[i].bdaddr.val[idx]);
            }
            printk("\n\r");
        }
    }
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

void main(void)
{

    int err;

    const struct device *dev;

    bool led_is_on = true;

    struct bt_le_scan_param scan_param = {
        .type = BT_HCI_LE_SCAN_PASSIVE,
        .options = BT_LE_SCAN_OPT_CODED,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    struct bt_scan_init_param scan_init = {
        .connect_if_match = 0,
        .scan_param = &scan_param,
        .conn_param = NULL};

    printk("Starting Scanner Demo\n");

    dev = device_get_binding(LED0);
    if (dev == NULL)
    {
        return;
    }

    err = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
    if (err < 0)
    {
        return;
    }

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    local_mac_printout();

    local_mac_reg_printout();
    local_mac_idget_printout();

    bt_scan_init(&scan_init);

    bt_scan_cb_register(&scan_cb);

    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, dev_name);
    if (err)
    {
        printk("Scanning filters cannot be set (err %d)\n", err);

        return;
    }

    err = bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false);
    if (err)
    {
        printk("Filters cannot be turned on (err %d)\n", err);
    }

    bt_scan_start(BT_SCAN_TYPE_SCAN_PASSIVE);

    do
    {
        gpio_pin_set(dev, PIN, (int)led_is_on);
        led_is_on = !led_is_on;

        k_sleep(K_MSEC(500));

    } while (1);
}