"""Generate The Saga Cutter soundtrack with the ElevenLabs Music API.

Setup (once):
    py -m pip install --user -r Scripts/requirements.txt
    The API key lives in .env.local (created by `npx @elevenlabs/cli onboard init`).

Usage:
    py Scripts/generate_music.py                 # all tracks, 1 take each
    py Scripts/generate_music.py --takes 3       # 3 takes per track
    py Scripts/generate_music.py --only menu action

Files land in Docs/Audio/Music/ (outside UE/Content so Unreal doesn't
auto-import every take). Import the take you like into UE and tick Looping.
"""

import argparse
import sys
import time
from pathlib import Path

from dotenv import load_dotenv

ROOT = Path(__file__).resolve().parent.parent
# Must run before importing elevenlabs: the SDK reads ELEVENLABS_API_KEY as a
# default argument, which Python evaluates at import time.
load_dotenv(ROOT / ".env.local")

from elevenlabs import ElevenLabs  # noqa: E402

OUT_DIR = ROOT / "Docs" / "Audio" / "Music"
MODEL_ID = "music_v2_5"
OUTPUT_FORMAT = "mp3_48000_192"

STYLE = (
    "Whimsical cozy Viking folk for an indie video game, like a children's "
    "storybook saga about a small fluffy Norse chieftain. Warm, golden, "
    "handcrafted, intimate acoustic mix, not dark or aggressive, no electronic "
    "elements."
)

TRACKS = {
    "main_theme": {
        "length_ms": 90_000,
        "instrumental": True,
        "prompt": (
            f"{STYLE} Main adventure theme, playful and heroic. Instruments: "
            "plucked Nordic lyre and tagelharpa melody, nyckelharpa and fiddle "
            "countermelody, bright wooden flute and tin whistle, soft frame drum "
            "and bodhran with bouncy stomping rhythm, gentle low wordless hum "
            "choir pad, light pizzicato strings, small glockenspiel sparkles, "
            "warm cello drones. Cheerful, brave, mischievous, cuddly, touch of "
            "epic pride. Medieval Scandinavian Dorian melody, simple hummable "
            "hook. Around 110 BPM march-like dance groove. Intro on lyre, hook "
            "on flute, fuller section with drums and fiddle, softer bridge, "
            "return to hook. Clean ending that loops seamlessly to the start."
        ),
    },
    "menu": {
        "length_ms": 75_000,
        "instrumental": True,
        "prompt": (
            f"{STYLE} Calm menu music, slow 80 BPM. Solo plucked lyre and soft "
            "wooden flute over a warm cello drone, gentle and nostalgic like a "
            "tale told by the fireplace. Sparse, peaceful, seamless loop."
        ),
    },
    "action": {
        "length_ms": 75_000,
        "instrumental": False,  # keep the wordless group shouts
        "prompt": (
            f"{STYLE} Energetic action track, 130 BPM. Driving frame drums and "
            "foot stomps, fast fiddle and tagelharpa riffs, flute runs, "
            "rhythmic wordless 'hey!' group shouts, no lyrics. Playful battle "
            "energy but still cute and lighthearted. Seamless loop."
        ),
    },
    "victory": {
        "length_ms": 10_000,
        "instrumental": True,
        "prompt": (
            f"{STYLE} Short triumphant victory fanfare: horn call, lyre "
            "flourish, big drum hit, ends on a bright held major chord."
        ),
    },
}


def generate(client: ElevenLabs, name: str, track: dict, take: int) -> Path:
    audio = client.music.compose(
        prompt=track["prompt"],
        music_length_ms=track["length_ms"],
        model_id=MODEL_ID,
        force_instrumental=track["instrumental"],
        output_format=OUTPUT_FORMAT,
    )
    data = b"".join(audio)  # the request runs here; fail before touching disk
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / f"{name}_take{take}.mp3"
    path.write_bytes(data)
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--takes", type=int, default=1, help="takes per track")
    parser.add_argument("--only", nargs="+", choices=TRACKS, help="subset of tracks")
    args = parser.parse_args()

    client = ElevenLabs()  # reads ELEVENLABS_API_KEY from the environment

    failures = 0
    for name in args.only or list(TRACKS):
        for take in range(1, args.takes + 1):
            print(f"[{name}] take {take}/{args.takes} ...", flush=True)
            start = time.time()
            try:
                path = generate(client, name, TRACKS[name], take)
                print(f"  saved {path.relative_to(ROOT)} ({time.time() - start:.0f}s)")
            except Exception as e:  # SDK raises ApiError with status/body
                failures += 1
                print(f"  failed: {e}", file=sys.stderr)
                if getattr(e, "status_code", None) in (401, 403):
                    return 1  # bad key / plan doesn't allow music: stop early

    print(f"Done. Output folder: {OUT_DIR.relative_to(ROOT)}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
