# Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
# See LICENSE file in project root for terms.

import serial

ser = serial.Serial('/dev/cu.usbmodem14201')
ser.write(b'debug sound on\n')
resp = ser.read(66)
print(resp)
assert resp == b'debug sound on\r\n\x1b[31mP\x1b[33ma\x1b[32mr\x1b[36ma\x1b[34mn\x1b[35mo\x1b[37mi\x1b[0md!> '

# About 10s
BLOCKS = 160

outputfile = open('test.raw', 'wb')

for _ in range(BLOCKS):
	block = ser.read(2048)
	# print(block)
	outputfile.write(block)

ser.write(b'debug sound off\n')
outputfile.close()
