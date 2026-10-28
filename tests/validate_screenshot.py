"""CI-only framebuffer checks; this script is not executed on the development PC."""
import sys
from PIL import Image, ImageStat

image = Image.open(sys.argv[1]).convert("RGB")
assert image.size == (1280, 800), f"Unexpected framebuffer dimensions: {image.size}"
stats = ImageStat.Stat(image)
assert max(stats.stddev) > 20, f"Framebuffer is blank or nearly uniform: {stats.stddev}"
colors = image.resize((320, 200)).getcolors(320 * 200)
assert colors is None or len(colors) > 200, "Too few rendered colors"
bright = sum(1 for p in image.getdata() if max(p) > 150)
assert bright > 4000, f"HUD / city highlights missing: {bright} pixels"
assert sum(stats.mean) > 45, f"Framebuffer is unexpectedly dark: {stats.mean}"
print(f"Framebuffer verified: {image.size}, RGB mean={stats.mean}, bright pixels={bright}")
