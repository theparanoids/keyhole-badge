// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#pragma once

int setup_buttons();
int read_all_buttons();

#define EYE_MAX_VAL 1024
int setup_eyes();
void set_left_eye(int r, int g, int b);
void set_right_eye(int r, int g, int b);

#define NLEDS 21
int setup_leds();
void set_led(int idx, int r, int g, int b);
void update_leds();
