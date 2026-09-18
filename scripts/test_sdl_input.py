#!/usr/bin/env python3
"""Run a Debug SDL build headlessly against power/modal input scenarios."""
import argparse
import os
from pathlib import Path
import subprocess

from PIL import Image, ImageChops


def run(build, output, name, keys, width=360, expected=0, shot_ms=6500):
    shot = output / f"{name}-{width}.bmp"
    shot.unlink(missing_ok=True)
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", EPASS_AUTOKEY=keys,
               EPASS_SHOT=str(shot), EPASS_SHOT_MS=str(shot_ms))
    result = subprocess.run([str(build / f"app_pc_{width}")], cwd=build,
                            env=env, capture_output=True, timeout=12)
    (output / f"{name}-{width}.log").write_bytes(result.stdout + result.stderr)
    assert result.returncode == expected, (name, result.returncode, expected)
    if expected == 0:
        assert shot.exists(), name
        image = Image.open(shot)
        image.save(output / f"{name}-{width}.png")
        height = image.height
        panel = image.crop((0, height - 125 * width // 360, width, height))
    else:
        assert not shot.exists(), name
        panel = None
    print(f"PASS {name} {width}: exit={result.returncode}", flush=True)
    return panel


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    reference = run(build, output, "shutdown", "1500:end:1700")
    run(build, output, "shutdown", "1500:end:1700", width=720)
    ignored = run(build, output, "ignored-keys",
                  "1500:end:1700,4800:esc,5150:left,5500:right")
    assert ImageChops.difference(reference, ignored).getbbox() is None
    held = run(build, output, "held-enter",
               "1500:end:100,3000:enter:5000,6000:end:1700", shot_ms=8500)
    assert ImageChops.difference(reference, held).getbbox() is None
    short = run(build, output, "short-menu", "1500:end:100")
    assert ImageChops.difference(reference, short).getbbox() is not None
    menu_back = run(build, output, "menu-back", "1500:end:100,4000:end:100")
    parked_panel = (0, 65, 360, 125)
    assert menu_back.crop(parked_panel).getbbox() is None
    run(build, output, "double-displayimg", "5000:end:80,5200:end:80")
    double_log = (output / "double-displayimg-360.log").read_text()
    assert double_log.count("[sdl] power: displayimg") == 1
    assert "[sdl] power: menu" not in double_log
    assert "[sdl] power: shutdown" not in double_log
    display_back = run(build, output, "displayimg-back",
                       "5000:end:80,5200:end:80,8000:end:100", shot_ms=10000)
    assert display_back.crop(parked_panel).getbbox() is None
    second_long = run(build, output, "double-second-long",
                      "5000:end:80,5200:end:1700", shot_ms=9000)
    assert ImageChops.difference(reference, second_long).getbbox() is None
    long_log = (output / "double-second-long-360.log").read_text()
    assert long_log.count("[sdl] power: shutdown") == 1
    assert "[sdl] power: menu" not in long_log
    assert "[sdl] power: displayimg" not in long_log
    cancel = run(build, output, "cancel", "1500:end:1700,5000:end:100")
    assert ImageChops.difference(reference, cancel).getbbox() is not None
    run(build, output, "confirm", "1500:end:1700,5000:enter", expected=3)
    print("All SDL input scenarios passed.")


if __name__ == "__main__":
    main()
