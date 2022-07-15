// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include "radio.h"

// This is a trick borrowed from DCFurs
// Android phones cannot set this value via the API, so we can prevent
// trivial badge spoofing via an app
#define MAGIC_APPEARANCE "Y!"
#define MAGIC_APPEARANCE_EASTEREGG "?R"

// We cheat and don't put in a manufacturer ID
#define MAGIC_MANUF_DATA	"Stay paranoid!"
#define MAGIC_MANUF_DATA_LEN (sizeof(MAGIC_MANUF_DATA) - 1)

// Friendly name
#define DEVICE_NAME "Paranoids Badge"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// Advertising payload
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_GAP_APPEARANCE, MAGIC_APPEARANCE_EASTEREGG, 2),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, MAGIC_MANUF_DATA, MAGIC_MANUF_DATA_LEN),
};

// Scan response payload
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

// <3 DCFurs badges
#define DEVICE_NAME_DCFURS "Paranoids <3 DCFurs"
#define DEVICE_NAME_DCFURS_LEN (sizeof(DEVICE_NAME_DCFURS) - 1)

#define MAGIC_MANUF_DATA_DC26	"\xFF\x71\xB2\x00\x00Y!"
#define MAGIC_MANUF_DATA_DC26_LEN (sizeof(MAGIC_MANUF_DATA_DC26) - 1)

#define MAGIC_MANUF_DATA_DC27	"\xFF\x71\xB2\x7FY!"
#define MAGIC_MANUF_DATA_DC27_LEN (sizeof(MAGIC_MANUF_DATA_DC27) - 1)

// Advertising payload
static const struct bt_data ad_dc26[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_GAP_APPEARANCE, "\xDC\x26", 2),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, MAGIC_MANUF_DATA_DC26, MAGIC_MANUF_DATA_DC26_LEN),
};

// Scan response payload
static const struct bt_data sd_dc26[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME_DCFURS, DEVICE_NAME_DCFURS_LEN),
};

// Advertising payload
static const struct bt_data ad_dc27[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_GAP_APPEARANCE, "\xDC\x27", 2),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, MAGIC_MANUF_DATA_DC27, MAGIC_MANUF_DATA_DC27_LEN),
};

// Scan response payload
static const struct bt_data sd_dc27[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME_DCFURS, DEVICE_NAME_DCFURS_LEN),
};


// Advertising sets
struct bt_le_ext_adv *ble_adv_sets[3];


// Peer tracking
K_MUTEX_DEFINE(peer_info_mutex);
struct peer_info {
	int age;
	// 0 = empty
	// 1 = not correct (no appearance field)
	// 2 = badge
	// 3 = easteregg
	// 4 = not correct (e.g. Android phone)
	uint8_t type;
	bt_addr_le_t addr;
};
#define NUM_TRACKED_PEERS 10
// in units of 5 seconds
#define PEER_MAX_AGE 12
static struct peer_info peers[NUM_TRACKED_PEERS];

static int num_peers;
static int num_imposters;
static int num_badge_makers;

struct adv_parse_state {
	// 0 = no
	// 1 = yes, badge
	// 2 = yes, easteregg
	// 3 = yes, not correct (e.g. Android phone)
	uint8_t found_appearance;
	// 0 = no
	// 1 = yes, valid, ours
	// 2 = yes, not correct (not relevant to us)
	uint8_t found_manuf_data;
};

static bool parse_adv_data(struct bt_data *data, void *state_) {
	struct adv_parse_state *state = state_;

	switch (data->type) {
		case BT_DATA_GAP_APPEARANCE:
			if (data->data_len != 2) {
				state->found_appearance = 3;
			} else {
				if (!memcmp(data->data, MAGIC_APPEARANCE, 2)) {
					state->found_appearance = 1;
				} else if (!memcmp(data->data, MAGIC_APPEARANCE_EASTEREGG, 2)) {
					state->found_appearance = 2;
				} else {
					state->found_appearance = 3;
				}
			}
			break;

		case BT_DATA_MANUFACTURER_DATA:
			if (data->data_len == MAGIC_MANUF_DATA_LEN) {
				if (!memcmp(data->data, MAGIC_MANUF_DATA, MAGIC_MANUF_DATA_LEN)) {
					state->found_manuf_data = 1;
				} else {
					state->found_manuf_data = 2;
				}
			} else {
				state->found_manuf_data = 2;
			}
			break;

		default:
			break;
	}

	return !state->found_appearance || !state->found_manuf_data;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, dev, sizeof(dev));

	// check if it's one of ours
	struct adv_parse_state state = {0};
	bt_data_parse(ad, parse_adv_data, &state);

	if (state.found_manuf_data == 1) {
		const char *desc;
		switch (state.found_appearance) {
			case 1:
				desc = "badge";
				break;
			case 2:
				desc = "badge maker's badge";
				break;
			default:
				desc = "spoofed badge";
				break;
		}
		printk("[DEVICE]: %s -- %s RSSI %i\n", dev, desc, rssi);

		k_mutex_lock(&peer_info_mutex, K_FOREVER);
		int idx_first_empty = -1;
		int idx_oldest = -1;
		int age_oldest = -1;
		int found_existing = 0;
		for (int i = 0; i < NUM_TRACKED_PEERS; i++) {
			if (peers[i].type) {
				if (!bt_addr_le_cmp(addr, &peers[i].addr)) {
					printk(" -> found @ %d\n", i);
					found_existing = 1;
					peers[i].age = 0;
					break;
				}

				if (peers[i].age > age_oldest) {
					age_oldest = peers[i].age;
					idx_oldest = i;
				}
			} else {
				if (idx_first_empty == -1)
					idx_first_empty = i;
			}
		}

		if (!found_existing) {
			if (idx_first_empty != -1) {
				printk(" -> peer #%d\n", idx_first_empty);
				peers[idx_first_empty].type = state.found_appearance + 1;
				peers[idx_first_empty].age = 0;
				bt_addr_le_copy(&peers[idx_first_empty].addr, addr);
				num_peers++;

				if (state.found_appearance == 2)
					num_badge_makers++;
				else if (state.found_appearance != 1)
					num_imposters++;
			} else {
				if (idx_oldest == -1) {
					printk("BUG BUG BUG, dropping discovered peer\n");
				} else {
					printk(" -> replacing oldest @ %d (age %d)\n", idx_oldest, age_oldest);

					if (peers[idx_oldest].type == 3)
						num_badge_makers--;
					else if (peers[idx_oldest].type != 2)
						num_imposters--;

					if (state.found_appearance == 2)
						num_badge_makers++;
					else if (state.found_appearance != 1)
						num_imposters++;

					peers[idx_oldest].type = state.found_appearance + 1;
					peers[idx_oldest].age = 0;
					bt_addr_le_copy(&peers[idx_oldest].addr, addr);
				}
			}
		}
		printk("NOW %d peers (%d imposters %d badge makers)\n", num_peers, num_imposters, num_badge_makers);
		k_mutex_unlock(&peer_info_mutex);
	}
}

#define BT_LE_ADV_NCONN_SCANNABLE BT_LE_ADV_PARAM(BT_LE_ADV_OPT_SCANNABLE, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_le_ext_adv_create(BT_LE_ADV_NCONN_SCANNABLE, 0, &ble_adv_sets[0]);
	if (err) {
		printk("Advertising set 0 failed to create (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_set_data(ble_adv_sets[0], ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising set 0 failed to set data (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_start(ble_adv_sets[0], BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Advertising set 0 failed to start (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_create(BT_LE_ADV_NCONN_SCANNABLE, 0, &ble_adv_sets[1]);
	if (err) {
		printk("Advertising set 1 failed to create (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_set_data(ble_adv_sets[1], ad_dc26, ARRAY_SIZE(ad_dc26), sd_dc26, ARRAY_SIZE(sd_dc26));
	if (err) {
		printk("Advertising set 1 failed to set data (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_start(ble_adv_sets[1], BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Advertising set 1 failed to start (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_create(BT_LE_ADV_NCONN_SCANNABLE, 0, &ble_adv_sets[2]);
	if (err) {
		printk("Advertising set 2 failed to create (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_set_data(ble_adv_sets[2], ad_dc27, ARRAY_SIZE(ad_dc27), sd_dc27, ARRAY_SIZE(sd_dc27));
	if (err) {
		printk("Advertising set 2 failed to set data (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_start(ble_adv_sets[2], BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Advertising set 2 failed to start (err %d)\n", err);
		return;
	}

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}
	printk("Scanning successfully started\n");
}

static void peer_aging(void *_0, void *_1, void *_2) {
	while (1) {
		printk("Aging peers...\n");

		k_mutex_lock(&peer_info_mutex, K_FOREVER);
		for (int i = 0; i < NUM_TRACKED_PEERS; i++) {
			if (peers[i].type) {
				peers[i].age++;
				if (peers[i].age >= PEER_MAX_AGE) {
					char dev[BT_ADDR_LE_STR_LEN];
					bt_addr_le_to_str(&peers[i].addr, dev, sizeof(dev));
					printk("Peer %s aged out\n", dev);

					if (peers[i].type == 3)
						num_badge_makers--;
					else if (peers[i].type != 2)
						num_imposters--;

					peers[i].type = 0;
					num_peers--;
				}
			}
		}
		printk("NOW %d peers (%d imposters %d badge makers)\n", num_peers, num_imposters, num_badge_makers);
		k_mutex_unlock(&peer_info_mutex);

		k_msleep(5000);
	}
}

K_THREAD_STACK_DEFINE(aging_thread_stack, 1024);
static struct k_thread aging_thread;

int badge_bt_setup() {
	int ret = bt_enable(bt_ready);
	if (ret) return ret;

	k_thread_create(
		&aging_thread,
		aging_thread_stack,
		K_THREAD_STACK_SIZEOF(aging_thread_stack),
		peer_aging,
		NULL, NULL, NULL,
		13, 0, K_NO_WAIT);

	return 0;
}

void get_peer_infos(int *out_num_peers, int *out_num_imposters, int *out_num_badge_makers) {
	k_mutex_lock(&peer_info_mutex, K_FOREVER);
	*out_num_peers = num_peers;
	*out_num_imposters = num_imposters;
	*out_num_badge_makers = num_badge_makers;
	k_mutex_unlock(&peer_info_mutex);
}
