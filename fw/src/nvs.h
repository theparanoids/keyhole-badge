// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#pragma once

int nvs_setup();

enum factory_mode {
	// R/G/B running around the ring (to check continuity and all colors working)
	// eyes red
	// waiting for SW1 to be pushed
	factory_before_sw1 = 0,

	// one white LED
	// eyes green
	// waiting for SW2 to be pushed
	factory_before_sw2 = 1,

	// two white LEDs
	// eyes blue
	// waiting for SW3 to be pushed
	factory_before_sw3 = 2,

	// three white LEDs
	// eyes white
	// waiting for SW4 to be pushed
	factory_before_sw4 = 3,

	// all white LEDs
	// eyes white
	// waiting for 440 Hz tone
	factory_before_mic_ok = 4,

	// no longer in production test mode
	factory_completed = 5,
};

enum factory_mode nvs_get_factory();
void nvs_set_factory(enum factory_mode mode);

uint32_t nvs_get_unlocked_blinky_patterns();
void nvs_set_unlocked_blinky_patterns(uint32_t patterns);
