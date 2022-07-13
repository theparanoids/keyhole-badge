// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#pragma once

int setup_sound();
int start_sound();
void process_sound(bool do_leds);
int process_sound_factory();
void sound_enable_debug(int enable);
void sound_enable_fft_debug(int enable);
