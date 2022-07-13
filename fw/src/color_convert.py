# Copyright Yahoo, Licensed under the terms of the Apache-2.0 license.
# See LICENSE file in project root for terms.

# Can either load this with `python -i` and call `srgb_to_linear` manually
# Or running it will generate the interpolated rainbow colors

def srgb_to_linear(sr, sg, sb, scale255=True, scale1024=False):
	sr /= 255
	sg /= 255
	sb /= 255

	if sr <= 0.04045:
		r = sr / 12.92
	else:
		r = ((sr + 0.055) / 1.055) ** 2.4

	if sg <= 0.04045:
		g = sg / 12.92
	else:
		g = ((sg + 0.055) / 1.055) ** 2.4

	if sb <= 0.04045:
		b = sb / 12.92
	else:
		b = ((sb + 0.055) / 1.055) ** 2.4

	if scale255:
		r = round(r * 255)
		g = round(g * 255)
		b = round(b * 255)

	if scale1024:
		r = round(r * 1024)
		g = round(g * 1024)
		b = round(b * 1024)

	return (r, g, b)

def linear_to_srgb(r, g, b):
	r /= 255
	g /= 255
	b /= 255

	if r <= 0.0031308:
		sr = r * 12.92
	else:
		sr = 1.055 * r ** (1 / 2.4) - 0.055

	if g <= 0.0031308:
		sg = g * 12.92
	else:
		sg = 1.055 * g ** (1 / 2.4) - 0.055

	if b <= 0.0031308:
		sb = b * 12.92
	else:
		sb = 1.055 * b ** (1 / 2.4) - 0.055

	sr = round(sr * 255)
	sg = round(sg * 255)
	sb = round(sb * 255)

	return (sr, sg, sb)

import skimage.color

COLOR_MALIBU = (255, 0, 128)
COLOR_TURMERIC_YELLOW = (255, 167, 0)
COLOR_MULAH_GREEN = (26, 197, 103)
COLOR_DORY_BLUE = (15, 105, 255)
# COLOR_GRAPE_JELLY = (91, 1, 210)
COLOR_GRAPE_JELLY = (117, 0, 210)

COLORS = [
	COLOR_MALIBU,
	COLOR_TURMERIC_YELLOW,
	COLOR_MULAH_GREEN,
	COLOR_DORY_BLUE,
	COLOR_GRAPE_JELLY,
]

for i in range(len(COLORS)):
	color = COLORS[i]
	next_color = COLORS[(i + 1) % len(COLORS)]
	print(srgb_to_linear(*color))

	color_lab = skimage.color.rgb2lab((color[0] / 255, color[1] / 255, color[2] / 255))
	next_color_lab = skimage.color.rgb2lab((next_color[0] / 255, next_color[1] / 255, next_color[2] / 255))
	# print("asdf", color_lab, next_color_lab)

	for j in range(1, 4):
		color_interp_lab = (next_color_lab - color_lab) * j / 4 + color_lab
		# print("fdsa", color_interp_lab)
		color_interp_srgb = skimage.color.lab2rgb(color_interp_lab) * 255
		# print(color_interp_srgb)
		print(srgb_to_linear(int(color_interp_srgb[0]), int(color_interp_srgb[1]), int(color_interp_srgb[2])))
