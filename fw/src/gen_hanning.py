# Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
# See LICENSE file in project root for terms.

import numpy as np

h = np.hanning(1024)

# only need half of it
h = h[:512]
h = h * 0x80000000

for j in range(512 // 8):
	for i in range(8):
		print(f"0x{int(h[j*8+i]):08x}, ", end='')
	print()
