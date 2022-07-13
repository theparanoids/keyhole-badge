// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#pragma once

#include <stdint.h>

int badge_nfc_setup();
int badge_nfc_set_msg_raw(const char *msg, uint32_t msg_len);
int badge_nfc_set_msg_puzzle(uint32_t unlocked_blinky_patterns);
