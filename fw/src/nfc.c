// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <zephyr.h>
#include <stdio.h>
#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

#include "nfc.h"
#include "nvs.h"

#define NDEF_MSG_BUF_SIZE	128
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];
static int is_running;

static char nfc_serno_buf[25];

int badge_nfc_set_msg_raw(const char *msg, uint32_t msg_len) {
	int ret;

	if (is_running) {
		ret = nfc_t2t_emulation_stop();
		if (ret) return ret;
	}

	// make new message
	size_t buf_len = NDEF_MSG_BUF_SIZE;
	NFC_NDEF_TEXT_RECORD_DESC_DEF(text_rec, UTF_8, "en", 2, msg, msg_len);
	NFC_NDEF_MSG_DEF(text_msg, 1);
	ret = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(text_msg), &NFC_NDEF_TEXT_RECORD_DESC(text_rec));
	if (ret) return ret;
	ret = nfc_ndef_msg_encode(&NFC_NDEF_MSG(text_msg), ndef_msg_buf, &buf_len);
	if (ret) return ret;

	// set it up and restart nfc (eww)
	ret = nfc_t2t_payload_set(ndef_msg_buf, buf_len);
	if (ret) return ret;

	ret = nfc_t2t_emulation_start();
	if (ret) return ret;

	return 0;
}

int badge_nfc_set_msg_puzzle(uint32_t unlocked_blinky_patterns) {
	snprintf(nfc_serno_buf, sizeof(nfc_serno_buf), "%08X%08X%08X", NRF_FICR->DEVICEID[0], NRF_FICR->DEVICEID[1], unlocked_blinky_patterns);
	return badge_nfc_set_msg_raw(nfc_serno_buf, sizeof(nfc_serno_buf) - 1);
}

static void nfc_callback(void *context,
			 nfc_t2t_event_t event,
			 const uint8_t *data,
			 size_t data_length)
{
}

int badge_nfc_setup() {
	int ret;

	ret = nfc_t2t_setup(nfc_callback, NULL);
	if (ret) return ret;

	ret = badge_nfc_set_msg_puzzle(nvs_get_unlocked_blinky_patterns());
	if (ret) return ret;

	is_running = 1;

	return 0;
}
