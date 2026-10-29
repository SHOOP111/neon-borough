"""CI-only framebuffer validation, with explicit checks even under python -O."""
import argparse
from PIL import Image, ImageStat


def validate(image, expected_size=(1280, 800)):
    if image.size != expected_size:
        raise ValueError(f"Unexpected framebuffer dimensions: {image.size}")
    image = image.convert("RGB")
    stats = ImageStat.Stat(image)
    if max(stats.stddev) <= 20:
        raise ValueError(f"Framebuffer is blank or nearly uniform: {stats.stddev}")
    colors = image.resize((320, 200)).getcolors(320 * 200)
    if colors is not None and len(colors) <= 200:
        raise ValueError("Too few rendered colors")
    bright = sum(1 for pixel in image.getdata() if max(pixel) > 150)
    if bright <= image.width * image.height * 0.0039:
        raise ValueError(f"HUD / city highlights missing: {bright} pixels")
    if sum(stats.mean) <= 45:
        raise ValueError(f"Framebuffer is unexpectedly dark: {stats.mean}")
    return f"Framebuffer verified: {image.size}, RGB mean={stats.mean}, highlights={bright}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=800)
    args = parser.parse_args()
    with Image.open(args.path) as image:
        print(validate(image, (args.width, args.height)))


if __name__ == "__main__":
    main()
