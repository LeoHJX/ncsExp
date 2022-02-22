/* Copyright (C) Shenzhen Minew Technologies Co., Ltd 
   All rights reserved. */

#include <zephyr.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <sys/util.h>

#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/controller.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(mt_advertising, 3);

static struct bt_le_ext_adv *adv_set;
/**

 */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
		      	0x56, 0x78, 			
		      	0x77, 0x77, 			
		      	0xC5),					
};
static const struct bt_data ad2[] = {
		
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, (sizeof(CONFIG_BT_DEVICE_NAME) - 1))
};
/**

 */

  bt_addr_le_t dir_addr;
const static struct bt_le_adv_param param =
		BT_LE_ADV_PARAM_INIT(
                     BT_LE_ADV_OPT_EXT_ADV|BT_LE_ADV_OPT_SCANNABLE|BT_LE_ADV_OPT_USE_IDENTITY|BT_LE_ADV_OPT_NOTIFY_SCAN_REQ|BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY,
				     BT_GAP_ADV_FAST_INT_MIN_2,
				     BT_GAP_ADV_FAST_INT_MIN_2,
                    //NULL);
				     &dir_addr);
static void adv_scanned_cb(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_scanned_info *info);

static struct bt_le_ext_adv_cb adv_callbacks = {
	.scanned = adv_scanned_cb,
};
static void adv_scanned_cb(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_scanned_info *info)
{
	printk("Scanned by ");
    for (uint8_t i=0;i<6;i++)    printk("%x ",info->addr->a.val[i]);
    printk("\n");
  //  LOG_INF("SCANNED");
}

/**

 */
void main(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("[%04d] Bluetooth init failed (err %d).\n", __LINE__, err);
        return;
    }

    const char *address = "F1:30:39:E6:12:B9";
    const char *type = "random";
    //const char *type = "public";
    if ((err = bt_addr_le_from_str(address, type, &dir_addr))) {
    printk("Bt_addr_le_from_str error: %d\n", err);
        }

    LOG_INF("[%04d] Bluetooth initialized.", __LINE__);
    err = bt_le_ext_adv_create(&param, &adv_callbacks, &adv_set);
   // err = bt_le_ext_adv_create(&param, NULL, &adv_set);
    if (err) {
        LOG_ERR("[%04d] Create extended advertising set_id failed (err %d).\n", __LINE__, err);
        return;
    }
    err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), ad2, ARRAY_SIZE(ad2));
    if (err) {
        LOG_ERR("[%04d] Failed to set advertising data (%d).\n", __LINE__, err);
        return;
    }
    err = bt_le_ext_adv_start(adv_set, NULL);
    if (err) {
        LOG_ERR("[%04d] Extended advertising failed to start (err %d).\n", __LINE__, err);
        return;
    }
    LOG_INF("[%04d] Extended advertising enable...", __LINE__);
}