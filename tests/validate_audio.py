"""CI-only validation of audio captured from a virtual PulseAudio monitor."""
import argparse
import array
import math
import pathlib
import sys


def validate(raw, expect_silent=False):
    if len(raw) % 4:
        raise ValueError("Partial floating-point audio sample")
    samples = array.array("f")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) <= 22050:
        raise ValueError(f"Insufficient captured audio: {len(samples)} frames")
    if not all(math.isfinite(x) for x in samples):
        raise ValueError("Nonfinite audio output")
    rms = math.sqrt(sum(x * x for x in samples) / len(samples))
    peak = max(abs(x) for x in samples)
    if expect_silent:
        if peak > 0.0001:
            raise ValueError(f"Muted output contains signal: peak {peak}")
    else:
        if rms <= 0.002:
            raise ValueError(f"Audio stream is silent: RMS {rms}")
        if peak >= 0.99:
            raise ValueError(f"Audio output clipped: peak {peak}")
        # Reject a brief click surrounded by silence, not just a global RMS check.
        windows = [samples[i:i + 2205] for i in range(0, len(samples), 2205)]
        audible = sum(math.sqrt(sum(x * x for x in w) / len(w)) > 0.002 for w in windows)
        if audible < 10:
            raise ValueError(f"Insufficient sustained audio: {audible} audible 100 ms windows")
    return f"Audio verified: {len(samples)} mono float frames, RMS={rms:.4f}, peak={peak:.4f}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=pathlib.Path)
    parser.add_argument("--expect-silent", action="store_true")
    args = parser.parse_args()
    print(validate(args.path.read_bytes(), args.expect_silent))


if __name__ == "__main__":
    main()
