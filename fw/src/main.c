// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <random/rand32.h>

#include "misc.h"
#include "nfc.h"
#include "nvs.h"
#include "radio.h"
#include "sound.h"
#include "usb.h"

// select from [0, n) without bias by rerolling "bad" results
static uint32_t rand_choice(uint32_t n) {
	uint64_t limit = 0x100000000ULL / n * n;

	while (1) {
		uint32_t ret = sys_rand32_get();
		if (ret < limit) {
			return ret % n;
		}
	}
}

// blinky blinky stuff
#define COLOR_GRAPE_JELLY		45, 0, 164
#define COLOR_SEA_FOAM			1, 166, 156
#define COLOR_DORY_BLUE			1, 36, 255
#define COLOR_DORY_TINT			101, 142, 246
#define COLOR_DORY_TINT_1024	406, 572, 988
#define COLOR_MALIBU			255, 0, 55
#define COLOR_MALIBU_TINT		237, 108, 154
#define COLOR_MALIBU_TINT_1024	952, 433, 618
#define COLOR_TURMERIC_YELLOW	255, 99, 0
#define COLOR_MULAH_GREEN		3, 142, 35

const uint8_t twinkle_colors_1[] = {
	COLOR_SEA_FOAM,
	COLOR_DORY_BLUE,
	COLOR_DORY_TINT,
};

const uint8_t sparkle_colors_2[] = {
	COLOR_GRAPE_JELLY,
	COLOR_DORY_TINT,
	COLOR_DORY_BLUE,
};

const uint8_t sparkle_colors_3[] = {
	COLOR_GRAPE_JELLY,
};

const uint8_t twinkle_colors_4[] = {
	COLOR_MALIBU,
	COLOR_TURMERIC_YELLOW,
	COLOR_MULAH_GREEN,
	COLOR_DORY_BLUE,
	COLOR_GRAPE_JELLY,
};

const uint16_t eye_colors_4[] = {
	COLOR_DORY_TINT_1024,
	COLOR_MALIBU_TINT_1024,
};

const uint16_t eye_colors_5[] = {
	214, 14, 1024	// Hulk pants
};

const uint8_t sparkle_colors_11[] = {
	COLOR_MALIBU,
	COLOR_TURMERIC_YELLOW,
	COLOR_MALIBU_TINT,
};

const uint8_t rainbow_cycle_colors[] = {
	255, 0, 55,
	255, 19, 38,
	255, 41, 24,
	255, 68, 11,
	255, 99, 0,
	177, 112, 6,
	111, 125, 13,
	52, 134, 23,
	3, 142, 35,
	16, 108, 71,
	20, 80, 118,
	16, 55, 179,
	1, 36, 255,
	14, 25, 229,
	26, 15, 206,
	36, 7, 184,
	45, 0, 164,
	45, 0, 164,
	92, 0, 130,
	142, 0, 101,
	196, 0, 76,
};

static void random_twinkle_loop(int num_colors, const uint8_t *colors, int delay) {
	const int max_leds = NLEDS;

	static int delay_remaining = 0;
	static int num_leds_lit = 0;

	if (delay_remaining-- >= 0) {} else {
		if (num_leds_lit == max_leds) {
			for (int i = 0; i < NLEDS; i++) {
				set_led(i, 0, 0, 0);
			}
			num_leds_lit = 0;
			delay_remaining = delay;
		} else {
			int led = rand_choice(NLEDS);
			const uint8_t *color = &colors[3 * rand_choice(num_colors)];
			set_led(led, color[0], color[1], color[2]);
			num_leds_lit++;
			delay_remaining = delay;
		}
	}
}

static void sparkle_loop(int num_colors, const uint8_t *colors, int use_eyes) {
	static int last_led_lit = -1;

	if (last_led_lit == -1) {
		// turn one on
		int led;
		if (use_eyes)
			led = rand_choice(NLEDS + 2);
		else
			led = rand_choice(NLEDS);
		const uint8_t *color = &colors[3 * rand_choice(num_colors)];

		if (led == NLEDS)
			set_left_eye(color[0] * 4, color[1] * 4, color[2] * 4);
		else if (led == NLEDS + 1)
			set_right_eye(color[0] * 4, color[1] * 4, color[2] * 4);
		else
			set_led(led, color[0], color[1], color[2]);
		last_led_lit = led;
	} else {
		// turn it off
		if (last_led_lit == NLEDS)
			set_left_eye(0, 0, 0);
		else if (last_led_lit == NLEDS + 1)
				set_right_eye(0, 0, 0);
		else
			set_led(last_led_lit, 0, 0, 0);
		last_led_lit = -1;
	}
}

static void color_wipe_loop() {
	// negative --> on the left
	// positive --> on the right
	static int num_not_lit = -NLEDS;

	if (num_not_lit <= 0) {
		for (int i = 0; i < -num_not_lit; i++) {
			set_led(i, 0, 0, 0);
		}
		for (int i = -num_not_lit; i < NLEDS; i++) {
			set_led(i, COLOR_GRAPE_JELLY);
		}

		num_not_lit++;
	}

	if (num_not_lit > 0) {
		for (int i = 0; i < NLEDS - num_not_lit; i++) {
			set_led(i, COLOR_GRAPE_JELLY);
		}
		for (int i = NLEDS - num_not_lit; i < NLEDS; i++) {
			set_led(i, 0, 0, 0);
		}

		if (num_not_lit == NLEDS)
			num_not_lit = -NLEDS;
		else
			num_not_lit++;
	}
}

static void eye_fade_loop(int num_colors, const uint16_t *colors, int reset) {
	const int num_steps = 16;

	// 0 = blank
	// 1 = up
	// 2 = down
	static int direction = 0;
	static int step = 0;
	static int color_idx = 0;

	if (reset) {
		direction = 0;
		step = 0;
		color_idx = 0;
	}

	const uint16_t *color = &colors[color_idx * 3];

	switch (direction) {
		case 0:
			set_left_eye(0, 0, 0);
			set_right_eye(0, 0, 0);
			if (++step == num_steps) {
				step = 0;
				direction = 1;
			}
			break;

		case 1:
			step++;
			set_left_eye(color[0] * step / num_steps, color[1] * step / num_steps, color[2] * step / num_steps);
			set_right_eye(color[0] * step / num_steps, color[1] * step / num_steps, color[2] * step / num_steps);
			if (step == num_steps) {
				direction = 2;
			}
			break;

		case 2:
			step--;
			set_left_eye(color[0] * step / num_steps, color[1] * step / num_steps, color[2] * step / num_steps);
			set_right_eye(color[0] * step / num_steps, color[1] * step / num_steps, color[2] * step / num_steps);
			if (step == 0) {
				direction = 0;
				color_idx = (color_idx + 1) % num_colors;
			}
			break;
	}
}

static void cylon_loop() {
	// 0 = right
	// 1 = left
	static int direction = 0;
	static int end_wait = 0;
	static int cylon_pos = 0;

	if (end_wait-- >= 0) {} else {
		for (int i = 0; i < NLEDS; i++)
			set_led(i, COLOR_GRAPE_JELLY);
		for (int i = 6; i <= 10; i++)
			set_led(i, 0, 0, 0);
		set_led(10 - cylon_pos, 163, 0, 4 /* cylon bar color */);

		if (direction == 0) {
			if (cylon_pos == 4) {
				direction = 1;
				end_wait = 1;
			} else
				cylon_pos++;
		} else {
			if (cylon_pos == 0) {
				direction = 0;
				end_wait = 1;
			} else
				cylon_pos--;
		}
	}
}

static void snow_sparkle_loop() {
	static int last_led_lit = -1;
	static int delay_remaining = 0;

	if (delay_remaining-- >= 0) {} else {
		for (int i = 0; i < NLEDS; i++)
			set_led(i, COLOR_GRAPE_JELLY);

		if (last_led_lit == -1) {
			// turn one on
			int led = rand_choice(NLEDS);
			set_led(led, 255, 255, 255);
			last_led_lit = led;
		} else {
			// turn it off (back to purple, which we already did above)
			last_led_lit = -1;
			delay_remaining = 6;
		}
	}
}

// fixme code duplication
static void fade_purples_loop() {
	const int num_steps = 16;

	// 0 = blank
	// 1 = up
	// 2 = down
	static int direction = 0;
	static int step = 0;
	static int color_idx = 0;

	switch (direction) {
		case 0:
			for (int i = 0; i < NLEDS; i++)
				set_led(i, 0, 0, 0);
			if (++step == num_steps) {
				step = 0;
				direction = 1;
			}
			break;

		case 1:
			step++;
			for (int i = 0; i < NLEDS; i++)
				// COLOR_GRAPE_JELLY
				set_led(i,
					45 * step / num_steps / (color_idx ? 4 : 1),
					0 * step / num_steps / (color_idx ? 4 : 1),
					164 * step / num_steps / (color_idx ? 4 : 1));
			if (step == num_steps) {
				direction = 2;
			}
			break;

		case 2:
			step--;
			for (int i = 0; i < NLEDS; i++)
				// COLOR_GRAPE_JELLY
				set_led(i,
					45 * step / num_steps / (color_idx ? 4 : 1),
					0 * step / num_steps / (color_idx ? 4 : 1),
					164 * step / num_steps / (color_idx ? 4 : 1));
			if (step == 0) {
				direction = 0;
				color_idx = (color_idx + 1) % 2;
			}
			break;
	}
}

static void fade_ukraine_loop() {
	const int num_steps = 16;

	// 0 = blank
	// 1 = up
	// 2 = down
	static int direction = 0;
	static int step = 0;

	switch (direction) {
		case 0:
			for (int i = 0; i < NLEDS; i++)
				set_led(i, 0, 0, 0);
			if (++step == num_steps) {
				step = 0;
				direction = 1;
			}
			break;

		case 1:
			step++;
			for (int i = 0; i < NLEDS; i++)
				if (i % 2 == 0)
					// COLOR_TURMERIC_YELLOW
					set_led(i,
						255 * step / num_steps,
						99 * step / num_steps,
						0 * step / num_steps);
				else
					// COLOR_DORY_BLUE
					set_led(i,
						1 * step / num_steps,
						36 * step / num_steps,
						255 * step / num_steps);
			if (step == num_steps) {
				direction = 2;
			}
			break;

		case 2:
			step--;
			for (int i = 0; i < NLEDS; i++)
				if (i % 2 == 0)
					// COLOR_TURMERIC_YELLOW
					set_led(i,
						255 * step / num_steps,
						99 * step / num_steps,
						0 * step / num_steps);
				else
					// COLOR_DORY_BLUE
					set_led(i,
						1 * step / num_steps,
						36 * step / num_steps,
						255 * step / num_steps);
			if (step == 0) {
				direction = 0;
			}
			break;
	}
}

static void rainbow_cycle_loop() {
	static int offset = 0;
	static int delay_remaining = 0;

	if (delay_remaining-- >= 0) {} else {
		for (int i = 0; i < NLEDS; i++) {
			const uint8_t *color = &rainbow_cycle_colors[((i + offset) % NLEDS) * 3];
			set_led(i, color[0], color[1], color[2]);
		}
		const uint8_t *color = &rainbow_cycle_colors[offset * 3];
		set_left_eye(color[0] * 4, color[1] * 4, color[2] * 4);
		set_right_eye(color[0] * 4, color[1] * 4, color[2] * 4);

		delay_remaining = 1;
		offset = (offset + 1) % NLEDS;
	}
}

static void radio_neighbor_eye_loop() {
	// fade in/out to random colors, where brightness
	// is controlled by the number of visible peers

	// if there are *any* imposters, right eye fades to red instead
	// if there are *any* easter egg badges, left eye fades to gold instead

	const int num_steps = 16;

	// 0 = blank
	// 1 = up
	// 2 = down
	static int direction = 2;
	static int step = 1;

	static int r, g, b;

	static int num_peers;
	static int num_imposters;
	static int num_badge_makers;

	int brightness_scale;
	if (num_peers == 0) {
		brightness_scale = 64;
	} else if (num_peers == 1) {
		brightness_scale = 8;
	} else if (num_peers == 2) {
		brightness_scale = 4;
	} else if (num_peers <= 5) {
		brightness_scale = 2;
	} else {
		brightness_scale = 1;
	}

	switch (direction) {
		case 0:
			set_left_eye(0, 0, 0);
			set_right_eye(0, 0, 0);
			if (++step == num_steps) {
				step = 0;
				direction = 1;
			}
			break;

		case 1:
			step++;
			set_left_eye(
				(num_badge_makers ? 1024 : r) * step / num_steps / brightness_scale,
				(num_badge_makers ? 696 : g) * step / num_steps / brightness_scale,
				(num_badge_makers ? 0 : b) * step / num_steps / brightness_scale);
			set_right_eye(
				(num_imposters ? 1024 : r) * step / num_steps / brightness_scale,
				(num_imposters ? 0 : g) * step / num_steps / brightness_scale,
				(num_imposters ? 0 : b) * step / num_steps / brightness_scale);
			if (step == num_steps) {
				direction = 2;
			}
			break;

		case 2:
			step--;
			set_left_eye(
				(num_badge_makers ? 1024 : r) * step / num_steps / brightness_scale,
				(num_badge_makers ? 696 : g) * step / num_steps / brightness_scale,
				(num_badge_makers ? 0 : b) * step / num_steps / brightness_scale);
			set_right_eye(
				(num_imposters ? 1024 : r) * step / num_steps / brightness_scale,
				(num_imposters ? 0 : g) * step / num_steps / brightness_scale,
				(num_imposters ? 0 : b) * step / num_steps / brightness_scale);
			if (step == 0) {
				direction = 0;

				// pick a new color
				int hue = rand_choice(6 * 1025);

				// awkward HSV-ish --> RGB, except S = 1 always
				int huebin = hue / 1025;
				int hue_in_bin = hue % 1025;

				switch (huebin) {
					case 0:
						r = 1024;
						g = hue_in_bin;
						b = 0;
						break;
					case 1:
						r = 1024 - hue_in_bin;
						g = 1024;
						b = 0;
						break;
					case 2:
						r = 0;
						g = 1024;
						b = hue_in_bin;
						break;
					case 3:
						r = 0;
						g = 1024 - hue_in_bin;
						b = 1024;
						break;
					case 4:
						r = hue_in_bin;
						g = 0;
						b = 1024;
						break;
					case 5:
					default:
						r = 1024;
						g = 0;
						b = 1024 - hue_in_bin;
						break;
				}

				// only update this when the leds fade out (no sudden changes)
				get_peer_infos(&num_peers, &num_imposters, &num_badge_makers);
			}
			break;
	}
}

#define NUM_PUZZLE_CODES	12
static const uint8_t PUZZLE_CODES[] = {
	2, 1, 1, 8,
	1, 5, 2, 5,
	2, 0, 2, 0,
	5, 3, 0, 9,
	9, 1, 2, 1,
	4, 2, 4, 2,
	1, 3, 3, 7,
	1, 2, 1, 5,
	1, 9, 9, 9,
	1, 2, 3, 4,
	6, 5, 4, 3,
	1, 1, 1, 1,
};
_Static_assert(sizeof(PUZZLE_CODES) == NUM_PUZZLE_CODES * 4, "Wrong NUM_PUZZLE_CODES");
_Static_assert(NUM_PUZZLE_CODES <= 32, "Too many codes");

// buttons from the *FRONT*
//   /      \
//  / 3    1 \
// / 4      2 \
// ------------

// should run at every 64 ms as long as we didn't take too long
static void game_loop(void) {
	// mode -1	==> puzzle code input
	// mode 0	==> sound/neighbor reactive mode
	// mode >=1	==> blinky blinky
	static int badge_main_mode = 0;

	static int did_init_unlocked_blinky_patterns = 0;
	static uint32_t unlocked_blinky_patterns = 0;

	if (!did_init_unlocked_blinky_patterns) {
		unlocked_blinky_patterns = nvs_get_unlocked_blinky_patterns();
		did_init_unlocked_blinky_patterns = 1;
	}

	static int last_buttons = 0;
	int this_buttons = read_all_buttons();

	int puzzle_pressed = !(last_buttons & 0b0010) && (this_buttons & 0b0010);
	int puzzle_held = this_buttons & 0b0010;

	static int puzzle_held_ticks;
	if (puzzle_pressed) {
		puzzle_held_ticks = 0;
	} else if (puzzle_held) {
		puzzle_held_ticks++;
	} else {
		puzzle_held_ticks = -1;
	}

	int mode_changed = 0;

	if (badge_main_mode > -1) {
		// button 2 ==> "right"
		// button 3 ==> "puzzle"
		// button 4 ==> "left"

		int left_pressed = !(last_buttons & 0b0001) && (this_buttons & 0b0001);
		int right_pressed = !(last_buttons & 0b0100) && (this_buttons & 0b0100);

		if (unlocked_blinky_patterns) {
			if (left_pressed && !right_pressed) {
				if (badge_main_mode) {
					// using a blinky right now
					// go down and find the next unlocked pattern
					// if there isn't one, go to mode 0
					int i;
					for (i = badge_main_mode - 1; i > 0; i--) {
						if (unlocked_blinky_patterns & (1 << (i - 1)))
							break;
					}
					badge_main_mode = i;
				} else {
					// sound-reactive mode right now
					// find highest set
					badge_main_mode = 32 - __builtin_clz(unlocked_blinky_patterns);

				}

				for (int i = 0; i < NLEDS; i++)
					set_led(i, 0, 0, 0);
				mode_changed = 1;
				printk("badge mode is now %d\n", badge_main_mode);
			}

			if (!left_pressed && right_pressed) {
				// go up and find the next unlocked pattern
				// if there isn't one, go to mode 0
				int i;
				for (i = badge_main_mode + 1; i <= NUM_PUZZLE_CODES; i++) {
					if (unlocked_blinky_patterns & (1 << (i - 1)))
						break;
				}

				if (i == NUM_PUZZLE_CODES + 1)
					badge_main_mode = 0;
				else
					badge_main_mode = i;

				for (int i = 0; i < NLEDS; i++)
					set_led(i, 0, 0, 0);
				mode_changed = 1;
				printk("badge mode is now %d\n", badge_main_mode);
			}
		}

		if (puzzle_held_ticks >= 78) {
			// held for 5 seconds
			badge_main_mode = -1;
			puzzle_held_ticks = 0;
			printk("badge mode is now %d\n", badge_main_mode);
		}
	}

	if (badge_main_mode == 0) {
		// this is the default sound/neighbor mode
		radio_neighbor_eye_loop();
		// XXX the *sound* processing is in sound.c, only the radio neighbor logic is here
	} else if (badge_main_mode == -1) {
		// this is the "puzzle input" mode
		set_left_eye(0, EYE_MAX_VAL - 1, 0);
		set_right_eye(0, EYE_MAX_VAL - 1, 0);

		if (puzzle_held_ticks >= 78) {
			// held for 5 seconds, cancel
			badge_main_mode = 0;
			puzzle_held_ticks = 0;
			printk("badge mode is now %d\n", badge_main_mode);
		}

		static int code0 = 0;
		static int code1 = 0;
		static int code2 = 0;
		static int code3 = 0;

		int code0_pressed = !(last_buttons & 0b0010) && (this_buttons & 0b0010);
		int code1_pressed = !(last_buttons & 0b0001) && (this_buttons & 0b0001);
		int code2_pressed = !(last_buttons & 0b1000) && (this_buttons & 0b1000);
		int code3_pressed = !(last_buttons & 0b0100) && (this_buttons & 0b0100);

		if (code0_pressed)
			code0 = (code0 + 1) % 10;
		if (code1_pressed)
			code1 = (code1 + 1) % 10;
		if (code2_pressed)
			code2 = (code2 + 1) % 10;
		if (code3_pressed)
			code3 = (code3 + 1) % 10;

		// printk("code is now %d%d%d%d\n", code0, code1, code2, code3);

		set_led(0, 0, 0, 0);

		set_led(1,
			code2 == 7 ? 255 : 0,
			code2 == 8 ? 255 : 0,
			code2 == 9 ? 255 : 0);
		set_led(2,
			code2 == 4 ? 255 : 0,
			code2 == 5 ? 255 : 0,
			code2 == 6 ? 255 : 0);
		set_led(3,
			code2 == 1 ? 255 : 0,
			code2 == 2 ? 255 : 0,
			code2 == 3 ? 255 : 0);

		set_led(4,
			code3 == 7 ? 255 : 0,
			code3 == 8 ? 255 : 0,
			code3 == 9 ? 255 : 0);
		set_led(5,
			code3 == 4 ? 255 : 0,
			code3 == 5 ? 255 : 0,
			code3 == 6 ? 255 : 0);
		set_led(6,
			code3 == 1 ? 255 : 0,
			code3 == 2 ? 255 : 0,
			code3 == 3 ? 255 : 0);

		set_led(7, 0, 0, 0);
		set_led(8, 0, 0, 0);
		set_led(9, 0, 0, 0);

		set_led(10,
			code1 == 1 ? 255 : 0,
			code1 == 2 ? 255 : 0,
			code1 == 3 ? 255 : 0);
		set_led(11,
			code1 == 4 ? 255 : 0,
			code1 == 5 ? 255 : 0,
			code1 == 6 ? 255 : 0);
		set_led(12,
			code1 == 7 ? 255 : 0,
			code1 == 8 ? 255 : 0,
			code1 == 9 ? 255 : 0);

		set_led(13,
			code0 == 1 ? 255 : 0,
			code0 == 2 ? 255 : 0,
			code0 == 3 ? 255 : 0);
		set_led(14,
			code0 == 4 ? 255 : 0,
			code0 == 5 ? 255 : 0,
			code0 == 6 ? 255 : 0);
		set_led(15,
			code0 == 7 ? 255 : 0,
			code0 == 8 ? 255 : 0,
			code0 == 9 ? 255 : 0);

		set_led(16, 0, 0, 0);
		set_led(17, 0, 0, 0);
		set_led(18, 0, 0, 0);
		set_led(19, 0, 0, 0);
		set_led(20, 0, 0, 0);

		int matched_code = -1;
		for (int i = 0; i < NUM_PUZZLE_CODES; i++) {
			if (code0 == PUZZLE_CODES[i*4+0] &&
				code1 == PUZZLE_CODES[i*4+1] &&
				code2 == PUZZLE_CODES[i*4+2] &&
				code3 == PUZZLE_CODES[i*4+3]) {
				printk("Matched code %d\n", i);
				matched_code = i;
				break;
			}
		}

		if (matched_code >= 0) {
			code0 = code1 = code2 = code3 = 0;

			unlocked_blinky_patterns |= (1 << matched_code);
			badge_main_mode = matched_code + 1;
			nvs_set_unlocked_blinky_patterns(unlocked_blinky_patterns);
			badge_nfc_set_msg_puzzle(unlocked_blinky_patterns);
			printk("unlocked patterns are now %08X\n", unlocked_blinky_patterns);
			for (int i = 0; i < NLEDS; i++)
				set_led(i, 0, 0, 0);
			printk("badge mode is now %d\n", badge_main_mode);
		}
	} else {
		// blinky blinky patterns

		switch (badge_main_mode) {
			case 1:
				// Random Twinkle with Sea Foam, Dory Blue, Dory Tint
				// with dory tint eyes
				random_twinkle_loop(3, twinkle_colors_1, 6);
				set_left_eye(COLOR_DORY_TINT_1024);
				set_right_eye(COLOR_DORY_TINT_1024);
				break;
			case 2:
				// Sparkle Grape Jelly, Dory Tint, Dory Blue
				sparkle_loop(3, sparkle_colors_2, 0);
				radio_neighbor_eye_loop();
				break;
			case 3:
				// Sparkle Grape Jelly
				// white eyes (low brightness)
				sparkle_loop(1, sparkle_colors_3, 0);
				set_left_eye(64, 64, 64);
				set_right_eye(64, 64, 64);
				break;
			case 4:
				// Random Twinkle Rainbow
				//  Malibu,
				//  Tumeric Yellow,
				//  Mulah green,
				//  Dory Blue,
				//  Grape Jelly
				// with Dory Tint and Malibu Tint fading to black and then to the next color for eyes
				random_twinkle_loop(5, twinkle_colors_4, 4);
				eye_fade_loop(2, eye_colors_4, mode_changed);
				break;
			case 5:
				// Color Wipe Grape Jelly
				// hulk pants eyes (slow fades)
				color_wipe_loop();
				eye_fade_loop(1, eye_colors_5, mode_changed);
				break;
			case 6:
				// Rainbow cycle
				rainbow_cycle_loop();
				break;
			case 7:
				// Cylon on bottom of the mascot with the rest of the logo lit Grape Jelly
				// and red eyes
				cylon_loop();
				set_left_eye(1024, 0, 0);
				set_right_eye(1024, 0, 0);
				break;
			case 8:
				// Snow Sparkle Grape Jelly - white eyes (low brightness) (so it’s on purple but has the sparkle highlight)
				snow_sparkle_loop();
				set_left_eye(64, 64, 64);
				set_right_eye(64, 64, 64);
				break;
			case 9:
				// Fading in and out Grape Jelly at a full (reasonable) brightness and the next one half that, slowly
				fade_purples_loop();
				radio_neighbor_eye_loop();
				break;
			case 10:
				// Ukraine Support mode
				// Alternate Tumeric and Dory every other light and fade them in and out slowly
				fade_ukraine_loop();
				radio_neighbor_eye_loop();
				break;
			case 11:
				// Sparkle Malibu, Tumeric Yellow, Malibu Tint
				// with eyes flickering those colors as well
				if (mode_changed) {
					set_left_eye(0, 0, 0);
					set_right_eye(0, 0, 0);
				}
				sparkle_loop(3, sparkle_colors_11, 1);
				break;
			case 12:
				// Plain black mascot (no light) with dim white eyes
				for (int i = 0; i < NLEDS; i++)
					set_led(i, 0, 0, 0);
				set_left_eye(64, 64, 64);
				set_right_eye(64, 64, 64);
				break;
		}
	}

	process_sound(badge_main_mode == 0);
	last_buttons = this_buttons;
	update_leds();
}

void main(void)
{
	int ret;

	if ((ret = nvs_setup())) {
		printk("NVS setup failed: %d\n", ret);
		return;
	}

	if ((ret = badge_usb_setup())) {
		printk("USB setup failed: %d\n", ret);
		return;
	}

	if ((ret = setup_eyes())) {
		printk("Eyes setup failed: %d\n", ret);
		return;
	}

	if ((ret = setup_leds())) {
		printk("LED setup failed: %d\n", ret);
		return;
	}

	if ((ret = setup_buttons())) {
		printk("Button setup failed: %d\n", ret);
		return;
	}

	if ((ret = badge_nfc_setup())) {
		printk("NFC setup failed: %d\n", ret);
		return;
	}

	if ((ret = badge_bt_setup())) {
		printk("Bluetooth setup failed: %d\n", ret);
		return;
	}

	if ((ret = setup_sound())) {
		printk("Sound setup failed: %d\n", ret);
		return;
	}

	enum factory_mode factory_mode_ = nvs_get_factory();

	if (factory_mode_ >= factory_before_mic_ok) {
		if ((ret = start_sound())) {
			printk("Sound start failed: %d\n", ret);
			return;
		}
	}

	int factory_chaser_idx = 0;

	while (1) {
		switch (factory_mode_) {
			case factory_before_sw1:
			default:
				set_left_eye(EYE_MAX_VAL - 1, 0, 0);
				set_right_eye(EYE_MAX_VAL - 1, 0, 0);

				for (int i = 0; i < NLEDS; i++) {
					if (i == factory_chaser_idx)
						set_led(i, 255, 0, 0);
					else if (i == (factory_chaser_idx + 1) % NLEDS)
						set_led(i, 0, 255, 0);
					else if (i == (factory_chaser_idx + 2) % NLEDS)
						set_led(i, 0, 0, 255);
					else
						set_led(i, 0, 0, 0);
				}
				update_leds();
				factory_chaser_idx = (factory_chaser_idx + 1) % NLEDS;

				if (read_all_buttons() & 0b1000) {
					factory_mode_ = factory_before_sw2;
					nvs_set_factory(factory_mode_);

					printk("Factory: SW1 pressed\n");
				}

				k_msleep(300);
				break;

			case factory_before_sw2:
				set_left_eye(0, EYE_MAX_VAL - 1, 0);
				set_right_eye(0, EYE_MAX_VAL - 1, 0);

				set_led(0, 255, 255, 255);
				for (int i = 1; i < NLEDS; i++) {
					set_led(i, 0, 0, 0);
				}
				update_leds();

				if (read_all_buttons() & 0b0100) {
					factory_mode_ = factory_before_sw3;
					nvs_set_factory(factory_mode_);

					printk("Factory: SW2 pressed\n");
				}
				break;

			case factory_before_sw3:
				set_left_eye(0, 0, EYE_MAX_VAL - 1);
				set_right_eye(0, 0, EYE_MAX_VAL - 1);

				set_led(0, 255, 255, 255);
				set_led(1, 255, 255, 255);
				for (int i = 2; i < NLEDS; i++) {
					set_led(i, 0, 0, 0);
				}
				update_leds();

				if (read_all_buttons() & 0b0010) {
					factory_mode_ = factory_before_sw4;
					nvs_set_factory(factory_mode_);
				
					printk("Factory: SW3 pressed\n");
				}
				break;

			case factory_before_sw4:
				set_left_eye(EYE_MAX_VAL - 1, EYE_MAX_VAL - 1, EYE_MAX_VAL - 1);
				set_right_eye(EYE_MAX_VAL - 1, EYE_MAX_VAL - 1, EYE_MAX_VAL - 1);

				set_led(0, 255, 255, 255);
				set_led(1, 255, 255, 255);
				set_led(2, 255, 255, 255);
				for (int i = 3; i < NLEDS; i++) {
					set_led(i, 0, 0, 0);
				}
				update_leds();

				if (read_all_buttons() & 0b0001) {
					factory_mode_ = factory_before_mic_ok;
					nvs_set_factory(factory_mode_);
				
					printk("Factory: SW4 pressed\n");

					if ((ret = start_sound())) {
						printk("Sound start failed: %d\n", ret);
						return;
					}
				}
				break;

			case factory_before_mic_ok:
				set_left_eye(EYE_MAX_VAL - 1, EYE_MAX_VAL - 1, EYE_MAX_VAL - 1);
				set_right_eye(EYE_MAX_VAL - 1, EYE_MAX_VAL - 1, EYE_MAX_VAL - 1);

				for (int i = 0; i < NLEDS; i++) {
					set_led(i, 255, 255, 255);
				}
				update_leds();

				int sound_passed = process_sound_factory();

				if (sound_passed) {
					factory_mode_ = factory_completed;
					nvs_set_factory(factory_mode_);

					printk("Factory: Mic check passed\n");
				}
				break;

			case factory_completed:
				game_loop();
				break;
		}
	}
}
