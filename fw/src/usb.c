// Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
// See LICENSE file in project root for terms.

#include <ctype.h>
#include <stdlib.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>
#include <drivers/uart.h>
#include <usb/usb_device.h>

#include "nfc.h"
#include "nvs.h"
#include "sound.h"
#include "usb.h"

K_PIPE_DEFINE(usb_rxpipe, 512, 1);
RING_BUF_DECLARE(usb_txring, 4096);

const struct device *const cdcacm_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

static void interrupt_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			int recv_len, rb_len;
			uint8_t buffer[64];
			size_t len = MIN(k_pipe_write_avail(&usb_rxpipe),
					 sizeof(buffer));

			recv_len = uart_fifo_read(dev, buffer, len);
			if (recv_len < 0) {
				printk("Failed to read CDCACM FIFO\r\n");
				recv_len = 0;
			};

			k_pipe_put(&usb_rxpipe, buffer, recv_len, &rb_len, 0, K_NO_WAIT);
			if (rb_len < recv_len) {
				printk("Drop %u rx bytes\r\n", recv_len - rb_len);
			}
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&usb_txring, buffer, sizeof(buffer));
			if (!rb_len) {
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				printk("Drop %d tx bytes\r\n", rb_len - send_len);
			}
		}
	}
}

static int echo_is_on;

static uint8_t rx_getchar() {
	uint8_t c[2];
	size_t sz;
	k_pipe_get(&usb_rxpipe, &c, 1, &sz, 1, K_FOREVER);
	if (echo_is_on) {
		if (c[0] == '\r' || c[0] == '\n') {
			// echo CRLF for either CR or LF
			c[0] = '\r';
			c[1] = '\n';
			badge_usb_write(c, 2);
		} else {
			badge_usb_write(c, 1);
		}
	}
	return c[0];
}

enum LineEditState {
	LineNone,
	LineSawCR,
	LineSawEsc,
	LineSawEscLBracket,
};

static uint32_t rx_getline(uint8_t *buf, uint32_t bufsz) {
	uint32_t pos = 0;
	uint32_t len = 0;
	enum LineEditState state = LineNone;

	while (1) {
		uint8_t c[3];
		size_t _;
		k_pipe_get(&usb_rxpipe, &c, 1, &_, 1, K_FOREVER);

		switch (state) {
			case LineSawCR:
				if (c[0] == '\n') {
					// This is a stray LF from the last line ending we saw, so deliberately ignore it
					// State goes back to normal though
					state = LineNone;
					break;
				}
				// deliberate fall through
			case LineNone:
				// Not processing any kind of escape sequence

				if (c[0] == '\r' || c[0] == '\n') {
					// Got a newline, we are done!

					if (echo_is_on) {
						// Gotta echo the newline though
						c[0] = '\r';
						c[1] = '\n';
						badge_usb_write(c, 2);
					}

					if (c[0] == '\r')
						state = LineSawCR;

					return len;
				}

				if (c[0] == '\x08' || c[0] == '\x7f') {
					// A backspace or a delete
					if (len == 0 || pos == 0)
						break;

					if (echo_is_on) {
	                	// Echo a backspace explicitly (screen doesn't like 0x7f)
	                	c[0] = '\x08';
	                	badge_usb_write(c, 1);
	                }

	                if (len != pos) {
	                    // Deleting a character from the middle, so we need to move everything left
	                    memmove(&buf[pos - 1], &buf[pos], len - pos);

	                    // Need to reprint everything too
	                    if (echo_is_on) {
	                    	badge_usb_write(&buf[pos - 1], len - pos);
		                }
	                }

	                if (echo_is_on) {
		                // Need this to actually wipe the (deleted/last) character away
		                c[0] = ' ';
		                c[1] = '\x08';
		                badge_usb_write(c, 2);

		                // Need to adjust the cursor back
		                c[0] = '\x08';
		                for (int _ = 0; _ < len - pos; _++) {
		                	badge_usb_write(c, 1);
		                }
		            }

	                pos -= 1;
	                len -= 1;
	                break;
				}

				if (c[0] == '\x1b') {
					// An escape
					state = LineSawEsc;
					break;
				}

	            // Just inserting a normal character

	            // If length is full, do nothing
	            if (len == bufsz) {
	                break;
	            }

	            if (len != pos) {
	                // Inserting something in the middle, so we need to move everything right
	                memmove(&buf[pos + 1], &buf[pos], len - pos);
	            }

	            buf[pos] = c[0];

	            if (echo_is_on) {
		            // Echo the character
		            badge_usb_write(c, 1);

		            // If a character was inserted, we have to print everything else too
		            if (len != pos) {
		            	badge_usb_write(&buf[pos + 1], len - pos);

		                // Now backspace to the same position
		                c[0] = '\x08';
		                for (int _ = 0; _ < len - pos; _++) {
		                	badge_usb_write(c, 1);
		                }
		            }
		        }

	            pos++;
	            len++;
	            break;
	        case LineSawEsc:
	            // Got ESC-[
	            if (c[0] == '[') {
	                state = LineSawEscLBracket;
	            } else {
	            	state = LineNone;
	            }
	            break;
	        case LineSawEscLBracket:
	            // Left arrow
	            if (c[0] == 'D') {
	                if (pos != 0) {
	                    pos--;

	                    if (echo_is_on) {
		                    // Echo the entire sequence
		                    c[0] = '\x1b';
		                    c[1] = '[';
		                    c[2] = 'D';
		                    badge_usb_write(c, 3);
		                }
	                }
	            }
	            // Right arrow
	            else if (c[0] == 'C') {
	                if (pos != len) {
	                    pos++;

	                    if (echo_is_on) {
		                    // Echo the entire sequence
		                    c[0] = '\x1b';
		                    c[1] = '[';
		                    c[2] = 'C';
		                    badge_usb_write(c, 3);
		                }
	                }
	            } else {
	                // Got ESC[-x which we don't understand. Just ignore it
	            }
	            state = LineNone;
	            break;
		}
	}
}

static void usb_putstr(const char *str) {
	badge_usb_write(str, strlen(str));
}

static void usb_rxthread(void *_0, void *_1, void *_2) {
	uint8_t line_buf[81];
	while (1) {
		uint32_t linelen = rx_getline(line_buf, sizeof(line_buf));
		line_buf[linelen] = 0;

		printk("got command line '%s'\n", line_buf);

		if (linelen > 0) {
			if (!strcmp(line_buf, "debug console echo off")) {
				echo_is_on = 0;
				usb_putstr("Echo is now off\r\n");
			} else if (!strcmp(line_buf, "debug console echo on")) {
				echo_is_on = 1;
				usb_putstr("Echo is now on\r\n");
			} else if (!strcmp(line_buf, "help")) {
				usb_putstr("Uhh, we didn't really finish this, sorry...\r\n");
				usb_putstr("\tdebug console echo [on|off] -- turn echo on/off\r\n");
				usb_putstr("\tdebug sound [on|off] -- turn sound raw data dump on/off\r\n");
				usb_putstr("\tdebug fft [on|off] -- turn fft raw data dump on/off\r\n");
				usb_putstr("\tdebug nfc set <msg> -- set the NFC NDEF message\r\n");
				usb_putstr("\tdebug set factory -- go into factory test mode\r\n");
				usb_putstr("\tdebug set no_factory -- go out of factory test mode\r\n");
				// don't show this one
				// usb_putstr("\tdebug __unlock_patterns <hex> -- override unlocked blinky patterns\r\n");
			} else if (!strcmp(line_buf, "debug sound on")) {
				sound_enable_debug(1);
			} else if (!strcmp(line_buf, "debug sound off")) {
				sound_enable_debug(0);
			} else if (!strcmp(line_buf, "debug fft on")) {
				sound_enable_fft_debug(1);
			} else if (!strcmp(line_buf, "debug fft off")) {
				sound_enable_fft_debug(0);
			} else if (!strncmp(line_buf, "debug nfc set ", strlen("debug nfc set "))) {
				badge_nfc_set_msg_raw(line_buf + strlen("debug nfc set "), linelen - strlen("debug nfc set "));
			} else if (!strcmp(line_buf, "debug set factory")) {
				nvs_set_factory(factory_before_sw1);
			} else if (!strcmp(line_buf, "debug set no_factory")) {
				nvs_set_factory(factory_completed);
			} else if (!strncmp(line_buf, "debug __unlock_patterns ", strlen("debug __unlock_patterns "))) {
				uint32_t patterns = strtol(line_buf + strlen("debug __unlock_patterns "), 0, 16);
				nvs_set_unlocked_blinky_patterns(patterns);
			} else {
				usb_putstr("Unrecognized command!\r\n");
			}
		}
		usb_putstr("\x1b[31mP\x1b[33ma\x1b[32mr\x1b[36ma\x1b[34mn\x1b[35mo\x1b[37mi\x1b[0md!> ");
	}
}

K_THREAD_STACK_DEFINE(rxthread_stack, 1024);
static struct k_thread rxthread;

int badge_usb_setup() {
	if (!device_is_ready(cdcacm_dev)) return -1;

	int ret = usb_enable(NULL);
	if (ret != 0) {
		return ret;
	}

	k_thread_create(
		&rxthread,
		rxthread_stack,
		K_THREAD_STACK_SIZEOF(rxthread_stack),
		usb_rxthread,
		NULL, NULL, NULL,
		14, 0, K_NO_WAIT);

	uart_irq_callback_set(cdcacm_dev, interrupt_handler);
	uart_irq_rx_enable(cdcacm_dev);

	echo_is_on = 1;

	return 0;
}

void badge_usb_write(const uint8_t *data, uint32_t sz) {
	while (sz) {
		uint32_t xferred = ring_buf_put(&usb_txring, data, sz);
		sz -= xferred;
		data += xferred;
		uart_irq_tx_enable(cdcacm_dev);
	}
}
