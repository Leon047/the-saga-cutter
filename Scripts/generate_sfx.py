"""Generate The Saga Cutter ocean sound effects with the ElevenLabs Sound Effects API.

Usage:
    py Scripts/generate_sfx.py                    # all sounds (2 takes of ocean_loop, 1 of the rest)
    py Scripts/generate_sfx.py --takes 3          # 3 takes per sound
    py Scripts/generate_sfx.py --only gull_1 boat_land
    py Scripts/generate_sfx.py --force            # regenerate files that already exist (spends credits)

Files land in Docs/Audio/SFX/ as 48 kHz 16-bit WAV (outside UE/Content so Unreal
doesn't auto-import them). Sounds that already exist on disk are skipped.
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

from dotenv import load_dotenv

ROOT = Path(__file__).resolve().parent.parent
# Must run before importing elevenlabs: the SDK reads ELEVENLABS_API_KEY as a
# default argument, which Python evaluates at import time.
# override=True: a stale ELEVENLABS_API_KEY in the shell must not shadow the file.
load_dotenv(ROOT / ".env.local", override=True)

from elevenlabs import ElevenLabs  # noqa: E402

OUT_DIR = ROOT / "Docs" / "Audio" / "SFX"
MODEL_ID = "eleven_text_to_sound_v2"
OUTPUT_FORMAT = "mp3_44100_128"
FFMPEG = "ffmpeg"
CROSSFADE_S = 1.5  # fallback loop seam when the API rejects loop=True

GULL = "A single distant seagull calling {}, far away, natural outdoor reverb, nothing else."
CREAK = (
    "Wooden ship hull creaking slowly as it rocks at its mooring, rope strain, "
    "close and dry, no water splash, no voices."
)
WADE = "One single footstep wading through knee-deep calm water, a small soft splash, close, nothing else."
RAVEN = "A single raven croaking {}, far away across a mountain valley, natural outdoor echo, nothing else."
OWL = "A single owl hooting {} in a quiet night forest, distant, natural outdoor reverb, nothing else."

SOUNDS = {
    "ocean_loop": {
        "seconds": 30, "loop": True, "channels": 2, "takes": 2,
        "prompt": (
            "Calm cold northern fjord, small waves washing steadily onto a pebble shore, "
            "soft continuous water movement, faint breeze. No birds, no voices, no music, "
            "no thunder. Even level, seamless loop."
        ),
    },
    "gull_1": {"seconds": 4, "loop": False, "channels": 1, "prompt": GULL.format("two or three times, one long cry")},
    "gull_2": {"seconds": 4, "loop": False, "channels": 1, "prompt": GULL.format("two or three times, short chattering cries")},
    "gull_3": {"seconds": 4, "loop": False, "channels": 1, "prompt": GULL.format("two or three times over open water")},
    "boat_creak_1": {"seconds": 3, "loop": False, "channels": 1, "prompt": CREAK},
    "boat_creak_2": {"seconds": 3, "loop": False, "channels": 1, "prompt": CREAK},
    "boat_land": {
        "seconds": 4, "loop": False, "channels": 1,
        "prompt": (
            "A wooden boat keel sliding onto a gravel beach with a soft splash of "
            "shallow water, then settling. No voices."
        ),
    },
    "wade_1": {"seconds": 2, "loop": False, "channels": 1, "prompt": WADE},
    "wade_2": {"seconds": 2, "loop": False, "channels": 1, "prompt": WADE},
    "river_loop": {
        "seconds": 30, "loop": True, "channels": 2, "takes": 2,
        "prompt": (
            "A small shallow river flowing steadily over stones, gentle continuous babbling water, "
            "close, cold northern valley. No birds, no voices, no music, no waves. Even level, seamless loop."
        ),
    },
    "wind_loop": {
        "seconds": 30, "loop": True, "channels": 2, "takes": 2,
        "prompt": (
            "Cold wind blowing steadily across a high mountain slope and through distant pines, "
            "soft airy whoosh with slow gentle swells. No gusty roar, no birds, no voices, no music. "
            "Even level, seamless loop."
        ),
    },
    "birds_loop": {
        "seconds": 30, "loop": True, "channels": 2, "takes": 2,
        "prompt": (
            "Quiet northern pine forest in the morning, a few small songbirds calling sparsely at a distance, "
            "light air. No water, no wind noise, no voices, no music. Even level, seamless loop."
        ),
    },
    "raven_1": {"seconds": 4, "loop": False, "channels": 1, "prompt": RAVEN.format("two or three times")},
    "raven_2": {"seconds": 4, "loop": False, "channels": 1, "prompt": RAVEN.format("one long hoarse croak")},
    "owl_1": {"seconds": 4, "loop": False, "channels": 1, "prompt": OWL.format("softly twice")},
    "owl_2": {"seconds": 4, "loop": False, "channels": 1, "prompt": OWL.format("three slow hoots")},
}


def request(client: ElevenLabs, snd: dict, fancy: bool) -> bytes:
    kwargs = dict(
        text=snd["prompt"],
        duration_seconds=snd["seconds"],
        prompt_influence=0.5,
        output_format=OUTPUT_FORMAT,
    )
    if fancy:
        kwargs.update(model_id=MODEL_ID, loop=snd["loop"])
    return b"".join(client.text_to_sound_effects.convert(**kwargs))


def to_wav(mp3: Path, wav: Path, channels: int, crossfade: bool) -> None:
    cmd = [FFMPEG, "-y", "-loglevel", "error", "-i", str(mp3)]
    if crossfade:
        # Drop the first d seconds from the front and crossfade them back onto the end,
        # so the end flows into the start.
        d = CROSSFADE_S
        flt = (
            f"[0:a]asplit=2[h][m];"
            f"[h]atrim=0:{d},asetpts=PTS-STARTPTS[head];"
            f"[m]atrim=start={d},asetpts=PTS-STARTPTS[rest];"
            f"[rest][head]acrossfade=d={d}[o]"
        )
        cmd += ["-filter_complex", flt, "-map", "[o]"]
    cmd += ["-ar", "48000", "-ac", str(channels), "-c:a", "pcm_s16le", str(wav)]
    subprocess.run(cmd, check=True)


def generate(client: ElevenLabs, name: str, snd: dict, take: int) -> Path:
    crossfade = False
    try:
        data = request(client, snd, fancy=True)
    except Exception as e:
        status = getattr(e, "status_code", None)
        if status in (401, 403, 429) or status is None or status >= 500:
            raise
        # loop / model_id rejected: plain request, build the loop ourselves.
        print(f"  loop/model rejected ({status}), retrying plain", file=sys.stderr)
        data = request(client, snd, fancy=False)
        crossfade = snd["loop"]
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    mp3 = OUT_DIR / f"{name}_take{take}.mp3"
    wav = OUT_DIR / f"{name}_take{take}.wav"
    mp3.write_bytes(data)
    try:
        to_wav(mp3, wav, snd["channels"], crossfade)
    finally:
        mp3.unlink(missing_ok=True)
    return wav


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--takes", type=int, help="takes per sound (default 2 for ocean_loop, 1 otherwise)")
    parser.add_argument("--only", nargs="+", choices=SOUNDS, help="subset of sounds")
    parser.add_argument("--force", action="store_true", help="regenerate files that already exist")
    args = parser.parse_args()

    client = ElevenLabs()  # reads ELEVENLABS_API_KEY from the environment

    failures = 0
    for name in args.only or list(SOUNDS):
        snd = SOUNDS[name]
        takes = args.takes or snd.get("takes", 1)
        for take in range(1, takes + 1):
            wav = OUT_DIR / f"{name}_take{take}.wav"
            if wav.exists() and not args.force:
                print(f"[{name}] take {take} exists, skipped")
                continue
            print(f"[{name}] take {take}/{takes} ...", flush=True)
            start = time.time()
            try:
                path = generate(client, name, snd, take)
                print(f"  saved {path.relative_to(ROOT)} ({time.time() - start:.0f}s)")
            except Exception as e:  # SDK raises ApiError with status/body
                failures += 1
                print(f"  failed: {e}", file=sys.stderr)
                if getattr(e, "status_code", None) in (401, 403):
                    return 1  # bad key / plan doesn't allow it: stop early

    print(f"Done. Output folder: {OUT_DIR.relative_to(ROOT)}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
