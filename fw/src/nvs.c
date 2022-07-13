// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <zephyr.h>
#include <device.h>
#include <storage/flash_map.h>
#include <fs/nvs.h>

#include "nvs.h"

static struct nvs_fs fs;

int nvs_setup() {
	const struct device *flash_dev = FLASH_AREA_DEVICE(storage);
	if (!device_is_ready(flash_dev)) {
		return -1;
	}
	fs.offset = FLASH_AREA_OFFSET(storage);
	fs.sector_size = 0x2000;
	fs.sector_count = 3;

	int ret = nvs_init(&fs, flash_dev->name);
	if (ret) {
		return ret;
	}

	return 0;
}

#define NVS_ID_FACTORY	1
#define NVS_ID_PATTERNS	2

enum factory_mode nvs_get_factory() {
	uint32_t mode = factory_before_sw1;
	int ret = nvs_read(&fs, NVS_ID_FACTORY, &mode, sizeof(mode));
	if (ret > 0) {
		printk("Factory mode: %d\n", mode);
	} else {
		printk("No factory mode found\n");
	}

	return mode;
}

void nvs_set_factory(enum factory_mode mode) {
	uint32_t mode_ = mode;
	(void)nvs_write(&fs, NVS_ID_FACTORY, &mode_, sizeof(mode_));
}

uint32_t nvs_get_unlocked_blinky_patterns() {
	uint32_t patterns = 0;
	int ret = nvs_read(&fs, NVS_ID_PATTERNS, &patterns, sizeof(patterns));
	if (ret > 0) {
		printk("NVS unlocked patterns: %08X\n", patterns);
	} else {
		printk("No NVS unlocked patterns found\n");
	}

	return patterns;
}

void nvs_set_unlocked_blinky_patterns(uint32_t patterns) {
	(void)nvs_write(&fs, NVS_ID_PATTERNS, &patterns, sizeof(patterns));
}
