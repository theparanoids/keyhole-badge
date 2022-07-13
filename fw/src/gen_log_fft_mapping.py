# Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
# See LICENSE file in project root for terms.

FFT_BIN_SPACING = 16000 / 1024
print(FFT_BIN_SPACING)

TOP_FREQ = 7812.5
LOG_SPACING = 1.22

for i in range(21, 0, -1):
	# print(i)
	freq_start = TOP_FREQ / (LOG_SPACING ** i)
	freq_end = TOP_FREQ / (LOG_SPACING ** (i - 1))
	print(f"frequencies {freq_start} Hz - {freq_end} Hz")

	bin_start = int(freq_start / FFT_BIN_SPACING)
	bin_end = int(freq_end / FFT_BIN_SPACING)
	print(f"bin {bin_start} to {bin_end}")
