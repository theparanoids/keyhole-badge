// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <drivers/spi.h>

#include "misc.h"

////////// BUTTONS //////////

static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(button1), gpios, {0});
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(button2), gpios, {0});
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(button3), gpios, {0});
static const struct gpio_dt_spec button4 = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(button4), gpios, {0});

int setup_buttons() {
	int ret;

	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret) return ret;
	ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
	if (ret) return ret;
	ret = gpio_pin_configure_dt(&button3, GPIO_INPUT);
	if (ret) return ret;
	ret = gpio_pin_configure_dt(&button4, GPIO_INPUT);
	if (ret) return ret;

	return 0;
}

int read_all_buttons() {
	int button1_val = gpio_pin_get_dt(&button1);
	int button2_val = gpio_pin_get_dt(&button2);
	int button3_val = gpio_pin_get_dt(&button3);
	int button4_val = gpio_pin_get_dt(&button4);

	return (!button1_val << 3) | (!button2_val << 2) | (!button3_val << 1) | (!button4_val);
}

////////// EYES //////////

#define LEFT_EYE_PWM_NODE DT_NODELABEL(left_eye_pwm)
#define LEFT_R_PWM_PIN DT_PROP(LEFT_EYE_PWM_NODE, ch0_pin)
#define LEFT_R_PWM_FLAGS DT_PWMS_FLAGS(LEFT_EYE_PWM_NODE)
#define LEFT_G_PWM_PIN DT_PROP(LEFT_EYE_PWM_NODE, ch1_pin)
#define LEFT_G_PWM_FLAGS DT_PWMS_FLAGS(LEFT_EYE_PWM_NODE)
#define LEFT_B_PWM_PIN DT_PROP(LEFT_EYE_PWM_NODE, ch2_pin)
#define LEFT_B_PWM_FLAGS DT_PWMS_FLAGS(LEFT_EYE_PWM_NODE)

#define RIGHT_EYE_PWM_NODE DT_NODELABEL(right_eye_pwm)
#define RIGHT_R_PWM_PIN DT_PROP(RIGHT_EYE_PWM_NODE, ch0_pin)
#define RIGHT_R_PWM_FLAGS DT_PWMS_FLAGS(RIGHT_EYE_PWM_NODE)
#define RIGHT_G_PWM_PIN DT_PROP(RIGHT_EYE_PWM_NODE, ch1_pin)
#define RIGHT_G_PWM_FLAGS DT_PWMS_FLAGS(RIGHT_EYE_PWM_NODE)
#define RIGHT_B_PWM_PIN DT_PROP(RIGHT_EYE_PWM_NODE, ch2_pin)
#define RIGHT_B_PWM_FLAGS DT_PWMS_FLAGS(LEFT_EYE_PWM_NODE)

static const struct device *const left_pwm_dev = DEVICE_DT_GET(LEFT_EYE_PWM_NODE);
static const struct device *const right_pwm_dev = DEVICE_DT_GET(RIGHT_EYE_PWM_NODE);

int setup_eyes() {
	if (!device_is_ready(left_pwm_dev)) return -1;
	if (!device_is_ready(right_pwm_dev)) return -1;

	return 0;
}

void set_left_eye(int r, int g, int b) {
	pwm_pin_set_usec(left_pwm_dev, LEFT_R_PWM_PIN, EYE_MAX_VAL, EYE_MAX_VAL - r, LEFT_R_PWM_FLAGS);
	pwm_pin_set_usec(left_pwm_dev, LEFT_G_PWM_PIN, EYE_MAX_VAL, EYE_MAX_VAL - g, LEFT_G_PWM_FLAGS);
	pwm_pin_set_usec(left_pwm_dev, LEFT_B_PWM_PIN, EYE_MAX_VAL, EYE_MAX_VAL - b, LEFT_B_PWM_FLAGS);
}

void set_right_eye(int r, int g, int b) {
	pwm_pin_set_usec(right_pwm_dev, RIGHT_R_PWM_PIN, EYE_MAX_VAL, EYE_MAX_VAL - r, RIGHT_R_PWM_FLAGS);
	pwm_pin_set_usec(right_pwm_dev, RIGHT_G_PWM_PIN, EYE_MAX_VAL, EYE_MAX_VAL - g, RIGHT_G_PWM_FLAGS);
	pwm_pin_set_usec(right_pwm_dev, RIGHT_B_PWM_PIN, EYE_MAX_VAL, EYE_MAX_VAL - b, RIGHT_B_PWM_FLAGS);
}

////////// MAIN LEDS //////////

#define LED_BUF_SZ	(NLEDS * 4 + 12)
static uint8_t led_buf[LED_BUF_SZ];

// [0-31]
#define LED_BRIGHTNESS	2

static const struct device *const spi_leds = DEVICE_DT_GET(DT_NODELABEL(spi_leds));

int setup_leds() {
	if (!device_is_ready(spi_leds)) return -1;
	return 0;
}

void set_led(int idx, int r, int g, int b) {
	led_buf[4 + idx*4 + 0] = 0xE0 | LED_BRIGHTNESS;
	led_buf[4 + idx*4 + 1] = b;
	led_buf[4 + idx*4 + 2] = g;
	led_buf[4 + idx*4 + 3] = r;
}

void update_leds() {
	struct spi_config spi_cfg = {0};
	spi_cfg.operation = SPI_WORD_SET(8);
	spi_cfg.frequency = 1000000;

	struct spi_buf bufs[] = {
		{
			.buf = led_buf,
			.len = sizeof(led_buf),
		},
	};

	struct spi_buf_set tx = {
		.buffers = bufs,
		.count = 1,
	};

	spi_write(spi_leds, &spi_cfg, &tx);
}
