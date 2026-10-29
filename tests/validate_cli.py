"""Exercise the compiled CLI without opening a window. CI-only."""
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="neon-cli-") as root:
    cases = [
        (["--help"], 0), (["-h"], 0),
        (["--help", "--seed", "18446744073709551615"], 0),
        (["--help", "--size", "1100x700"], 0),
        (["--help", "--no-save", "--no-audio"], 0),
        (["--unknown"], 2), (["--frames"], 2), (["--frames", "0"], 2),
        (["--frames", "20001"], 2), (["--frames", "1junk"], 2),
        (["--seed", "-1"], 2), (["--seed", "18446744073709551616"], 2),
        (["--seed", ""], 2), (["--save-dir"], 2), (["--screenshot"], 2),
        (["--smoke-view", "invalid"], 2), (["--size", "1x1"], 2),
        (["--size", "1100X700"], 2), (["--size", "1100x700junk"], 2),
        (["--size", "999999999999x800"], 2),
    ]
    for view in ("gameplay", "title", "map", "garage", "paused", "settings", "waypoint"):
        cases.append((["--help", "--smoke-view", view], 0))
    for args, expected in cases:
        result = subprocess.run([binary, *args], cwd=root, capture_output=True, text=True, timeout=10)
        if result.returncode != expected:
            raise RuntimeError(f"CLI {args}: expected {expected}, got {result.returncode}\n"
                               f"{result.stdout}\n{result.stderr}")
        if "Initializing raylib" in result.stdout:
            raise RuntimeError(f"CLI unexpectedly initialized a window: {args}")
    if list(pathlib.Path(root).iterdir()):
        raise RuntimeError("CLI validation unexpectedly wrote files")
print(f"CLI verified: {len(cases)} argument cases; no graphics or persistence initialized")
