"""Tests for CI evidence validators; executed only on GitHub-hosted runners."""
import math
import struct
import unittest
from PIL import Image
import validate_audio
import validate_screenshot


class AudioValidation(unittest.TestCase):
    def raw(self, values):
        return struct.pack(f"<{len(values)}f", *values)

    def test_sustained_tone(self):
        tone = [0.1 * math.sin(i * 0.2) for i in range(44100)]
        self.assertIn("verified", validate_audio.validate(self.raw(tone)))

    def test_rejects_bad_signal(self):
        for values in ([0.0] * 44100, [1.0] * 44100, [float("nan")] * 44100,
                       [float("inf")] * 44100, [0.1] * 22050,
                       [0.5] * 100 + [0.0] * 44000):
            with self.assertRaises(ValueError):
                validate_audio.validate(self.raw(values))
        with self.assertRaises(ValueError):
            validate_audio.validate(b"partial")

    def test_mute(self):
        self.assertIn("verified", validate_audio.validate(self.raw([0.0] * 44100), True))
        with self.assertRaises(ValueError):
            validate_audio.validate(self.raw([0.01] * 44100), True)


class ScreenshotValidation(unittest.TestCase):
    def test_rejects_blank_and_wrong_dimensions(self):
        for color in ("black", "white", (40, 40, 40)):
            with self.assertRaises(ValueError):
                validate_screenshot.validate(Image.new("RGB", (1280, 800), color))
        with self.assertRaises(ValueError):
            validate_screenshot.validate(Image.new("RGB", (1, 1)))

    def test_colored_frame_and_alternate_size(self):
        image = Image.new("RGB", (1280, 800))
        image.putdata([(i % 256, (i // 256) % 256, (i // 17) % 256)
                       for i in range(1280 * 800)])
        self.assertIn("verified", validate_screenshot.validate(image))
        small = image.resize((1100, 700))
        self.assertIn("verified", validate_screenshot.validate(small, (1100, 700)))


if __name__ == "__main__":
    unittest.main()
