"""CI-only validation of audio captured from a virtual PulseAudio monitor."""
import array
import math
import pathlib
import sys

raw = pathlib.Path(sys.argv[1]).read_bytes()
assert len(raw) % 4 == 0, "Partial floating-point audio sample"
samples = array.array("f")
samples.frombytes(raw)
if sys.byteorder != "little":
    samples.byteswap()
assert len(samples) > 22050, f"Insufficient captured audio: {len(samples)} frames"
assert all(math.isfinite(x) for x in samples), "Nonfinite audio output"
rms = math.sqrt(sum(x * x for x in samples) / len(samples))
peak = max(abs(x) for x in samples)
assert rms > 0.002, f"Audio stream is silent: RMS {rms}"
assert peak < 0.99, f"Audio output clipped: peak {peak}"
print(f"Audio verified: {len(samples)} mono float frames at 22050 Hz, RMS={rms:.4f}, peak={peak:.4f}")
