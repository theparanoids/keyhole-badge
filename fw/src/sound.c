// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <zephyr.h>
#include <devicetree.h>
#include <audio/dmic.h>
#include <hal/nrf_pdm.h>
#include <random/rand32.h>

#include "misc.h"
#include "sound.h"
#include "usb.h"

#include "hanning.h"

#include "SYLT-FFT/fft.h"

#define SAMPLE_RATE			16000
#define BYTES_PER_SAMPLE	sizeof(int16_t)
#define BITS_PER_SAMPLE		16

#define SAMPLES_LOG2		10
#define SAMPLES_PER_BLOCK	(1 << SAMPLES_LOG2)
#define BLOCK_SIZE			(SAMPLES_PER_BLOCK * BYTES_PER_SAMPLE)
#define BLOCK_COUNT			4
K_MEM_SLAB_DEFINE_STATIC(mem_slab, BLOCK_SIZE, BLOCK_COUNT, 4);

_Static_assert(sizeof(hanning_window) / sizeof(hanning_window[0]) == SAMPLES_PER_BLOCK / 2, "Wrong data size");

static const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm));

static int debug_enabled;
static int debug_fft_enabled;

// Accumulated data across loops
float fft_history[NLEDS];
// colors
int led_hues[NLEDS];

// FIXME code duplication
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

int setup_sound() {
	int ret;

	struct pcm_stream_cfg stream = {
		.pcm_width = BITS_PER_SAMPLE,
		.mem_slab  = &mem_slab,
	};
	struct dmic_cfg cfg = {
		.io = {
			.min_pdm_clk_freq = 1100000,
			.max_pdm_clk_freq = 4800000,
			.min_pdm_clk_dc   = 40,
			.max_pdm_clk_dc   = 60,
		},
		.streams = &stream,
		.channel = {
			.req_num_streams = 1,
		},
	};

	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_RIGHT);
	cfg.streams[0].pcm_rate = SAMPLE_RATE;
	cfg.streams[0].block_size = BLOCK_SIZE;

	ret = dmic_configure(dmic_dev, &cfg);
	if (ret) return ret;

    nrf_pdm_gain_set(NRF_PDM0, 0x50, 0x50);

	debug_enabled = 0;

	for (int i = 0; i < NLEDS; i++) {
		led_hues[i] = rand_choice(6 * 256);
	}

	return 0;
}

int start_sound() {
	return dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
}

// Yoink this function from the SYLT-FFT code, except modify it
// such that it copies data from the mem slab into a work buffer
// while performing the necessary permute
void badge_fft_permutate(fft_complex_t * restrict out, const int16_t * restrict in) {
	unsigned shift = 32 - SAMPLES_LOG2;
	for(unsigned i = 0; i < SAMPLES_PER_BLOCK; i++) {
		unsigned z = rbit(i) >> shift;

		int32_t win;
		if (i < 512)
			win = hanning_window[i];
		else
			win = hanning_window[1023 - i];

		out[z].r = smmulr(in[i] * 2, win);
		out[z].i = 0;
	}
}

// FFT work buffer (integer)
fft_complex_t sound_fft[SAMPLES_PER_BLOCK];
// FFT work buffer (each loop, float, logarithmic)
float fft_data_log[NLEDS];

static inline float mag_sq(fft_complex_t x) {
	return	(float)x.r * (float)x.r +
			(float)x.i * (float)x.i;
}

static void set_led_hsvish(int idx, int h, int v) {
	int r, g, b;

	// awkward HSV-ish --> RGB, except S = 1 always
	int huebin = h / 256;
	int hue_in_bin = h % 256;

	// hue_in_bin in [0, 255]

	switch (huebin) {
		case 0:
			r = v;
			g = v * hue_in_bin / 255;
			b = 0;
			break;
		case 1:
			r = v * (255 - hue_in_bin) / 255;
			g = v;
			b = 0;
			break;
		case 2:
			r = 0;
			g = v;
			b = v * hue_in_bin / 255;
			break;
		case 3:
			r = 0;
			g = v * (255 - hue_in_bin) / 255;
			b = v;
			break;
		case 4:
			r = v * hue_in_bin / 255;
			g = 0;
			b = v;
			break;
		case 5:
		default:
			r = v;
			g = 0;
			b = v * (255 - hue_in_bin) / 255;
			break;
	}

	set_led(idx, r, g, b);
}

void process_sound(bool do_leds) {
	void *buffer_;
	uint32_t size;
	int ret = dmic_read(dmic_dev, 0, &buffer_, &size, 1000);
	if (ret < 0) {
		printk("pdm - read failed: %d\n", ret);
		return;
	}
	if (size != BLOCK_SIZE) {
		printk("pdm - bad block size: %u\n", size);
		return;
	}

	int16_t *buffer = buffer_;
	badge_fft_permutate(sound_fft, buffer);

	if (debug_enabled)
		badge_usb_write(buffer_, size);

	k_mem_slab_free(&mem_slab, &buffer_);

	fft_forward(sound_fft, SAMPLES_LOG2);

	if (debug_fft_enabled)
		badge_usb_write((uint8_t *)&sound_fft, sizeof(sound_fft));

	// XXX we use FPU, guess that should be fine
	// (wrt both perf *and* RTOS bugs)

	// Convert FFT data into logarithmic bins for each LED
	float sum;

	sum = 0;
	for (int i = 7; i < 9; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[0] = sum;

	sum = 0;
	for (int i = 9; i < 11; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[1] = sum;

	sum = 0;
	for (int i = 11; i < 13; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[2] = sum;

	sum = 0;
	for (int i = 13; i < 17; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[3] = sum;

	sum = 0;
	for (int i = 17; i < 20; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[4] = sum;

	sum = 0;
	for (int i = 20; i < 25; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[5] = sum;

	sum = 0;
	for (int i = 25; i < 30; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[6] = sum;

	sum = 0;
	for (int i = 30; i < 37; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[7] = sum;

	sum = 0;
	for (int i = 37; i < 45; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[8] = sum;

	sum = 0;
	for (int i = 45; i < 56; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[9] = sum;

	sum = 0;
	for (int i = 56; i < 68; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[10] = sum;

	sum = 0;
	for (int i = 68; i < 83; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[11] = sum;

	sum = 0;
	for (int i = 83; i < 101; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[12] = sum;

	sum = 0;
	for (int i = 101; i < 124; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[13] = sum;

	sum = 0;
	for (int i = 124; i < 151; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[14] = sum;

	sum = 0;
	for (int i = 151; i < 184; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[15] = sum;

	sum = 0;
	for (int i = 184; i < 225; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[16] = sum;

	sum = 0;
	for (int i = 225; i < 275; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[17] = sum;

	sum = 0;
	for (int i = 275; i < 335; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[18] = sum;

	sum = 0;
	for (int i = 335; i < 409; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[19] = sum;

	sum = 0;
	for (int i = 409; i < 500; i++) sum += mag_sq(sound_fft[i]);
	fft_data_log[20] = sum;

	// update the history data
	for (int i = 0; i < NLEDS; i++) {
		// if more than 10% louder --> new color
		if (fft_data_log[i] > fft_history[i] * 1.1f) {
			led_hues[i] = rand_choice(6 * 256);
		}
	}

	for (int i = 0; i < NLEDS; i++) {
		// aging, calculated s.t. after ~0.5s we get 1% of old value
		fft_history[i] *= 0.55f;
	}

	for (int i = 0; i < NLEDS; i++) {
		// update history if we are louder
		if (fft_data_log[i] > fft_history[i]) {
			fft_history[i] = fft_data_log[i];
		}
	}

	// calculate max to scale colors
	float max_val = 0;
	for (int i = 0; i < NLEDS; i++) {
		if (fft_history[i] > max_val)
			max_val = fft_history[i];
	}

	if (do_leds) {
		for (int led = 0; led < NLEDS; led++) {
			// printk("bin[%d] = %d\n", led, (int)(fft_history[led]));
			uint32_t led_val = fft_history[led] / max_val * 0xFF;
			if (led_val > 0xFF) led_val = 0xFF;
			set_led_hsvish(led, led_hues[led], led_val);
		}
	}
}

// FIXME the code duplication is ugly
int process_sound_factory() {
	void *buffer_;
	uint32_t size;
	int ret = dmic_read(dmic_dev, 0, &buffer_, &size, 1000);
	if (ret < 0) {
		printk("pdm - read failed: %d\n", ret);
		return 0;
	}
	if (size != BLOCK_SIZE) {
		printk("pdm - bad block size: %u\n", size);
		return 0;
	}

	int16_t *buffer = buffer_;
	badge_fft_permutate(sound_fft, buffer);

	if (debug_enabled)
		badge_usb_write(buffer_, size);

	k_mem_slab_free(&mem_slab, &buffer_);

	fft_forward(sound_fft, SAMPLES_LOG2);

	printk("FFT [0] = %d %d\n", sound_fft[0].r, sound_fft[0].i);
	printk("FFT [10] = %d %d\n", sound_fft[10].r, sound_fft[10].i);
	printk("FFT [28] = %d %d\n", sound_fft[28].r, sound_fft[28].i);
	printk("FFT [29] = %d %d\n", sound_fft[29].r, sound_fft[29].i);

	// DC check is a workaround for weird mic data that shows up right after reset
	uint32_t m_dc = mag_sq(sound_fft[0]);
	uint32_t m_440 = mag_sq(sound_fft[28]);

	// arbitrary thresholds that seem to work ok
	return m_dc < 25 && m_440 > 100;
}

void sound_enable_debug(int enable) {
	debug_enabled = enable;
}

void sound_enable_fft_debug(int enable) {
	debug_fft_enabled = enable;
}
