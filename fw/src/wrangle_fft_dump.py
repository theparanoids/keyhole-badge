# Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
# See LICENSE file in project root for terms.

import numpy as np

fft = np.fromfile('test_fft.raw', np.int32)
fft_r = fft[::2]
fft_i = fft[1::2]
fft_c = fft_r + 1j * fft_i

ifft = np.array(0, dtype=np.float64)

for blki in range(len(fft_c) // 1024):
	fft_blk = fft_c[blki * 1024:(blki + 1) * 1024]
	# print(len(fft_blk))

	ifft_blk = np.real(np.fft.ifft(fft_blk))
	print(ifft_blk)

	ifft = np.concatenate((ifft, ifft_blk), axis=None)

ifft = ifft / np.max(np.abs(ifft))
ifft.astype('float32').tofile('test_ifft.raw')
