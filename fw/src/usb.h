// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#pragma once

#include <stdint.h>

int badge_usb_setup();
void badge_usb_write(const uint8_t *data, uint32_t sz);
