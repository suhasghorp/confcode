"""Stage 2: sample frames and keep one representative per stable screen.

Talks often show the speaker's camera next to the slides. A naive image
comparison sees the moving speaker as "the screen changed", so we first build
a motion mask: pixels that change in a large share of consecutive samples
(camera feeds, animated logos) are ignored when comparing frames.
"""

from __future__ import annotations

import subprocess
from collections.abc import Callable
from pathlib import Path

import numpy as np
from PIL import Image

from .models import Frame

THUMB = (192, 108)
PIXEL_DELTA = 24  # grey-level change that counts as "changed"


def sample(video: Path, out_dir: Path, interval: float, offset: float, width: int = 1280) -> list[Frame]:
    out_dir.mkdir(parents=True, exist_ok=True)
    for old in out_dir.glob("*.jpg"):
        old.unlink()
    subprocess.run(
        [
            "ffmpeg", "-loglevel", "error", "-i", str(video),
            "-vf", f"fps=1/{interval},scale={width}:-2",
            "-q:v", "2", str(out_dir / "%05d.jpg"),
        ],
        check=True,
    )
    files = sorted(out_dir.glob("*.jpg"))
    # ffmpeg's fps filter centres samples in each interval.
    return [
        Frame(index=i, ts=offset + (i + 0.5) * interval, path=str(p)) for i, p in enumerate(files)
    ]


def load_thumbs(frames: list[Frame]) -> np.ndarray:
    return np.stack([
        np.asarray(Image.open(f.path).convert("L").resize(THUMB, Image.BILINEAR), dtype=np.int16)
        for f in frames
    ])


def motion_mask(thumbs: np.ndarray, busy_share: float = 0.05) -> np.ndarray:
    """True for pixels that are static enough to compare (i.e. NOT camera/animation)."""
    if len(thumbs) < 3:
        return np.ones(thumbs.shape[1:], dtype=bool)
    changed = np.abs(np.diff(thumbs, axis=0)) > PIXEL_DELTA
    busy = changed.mean(axis=0) > busy_share
    # Dilate the busy area a little so the speaker's edges don't leak through.
    r = 2
    p = np.pad(busy, r)
    h, w = busy.shape
    b = np.zeros_like(busy)
    for dy in range(2 * r + 1):
        for dx in range(2 * r + 1):
            b |= p[dy : dy + h, dx : dx + w]
    return ~b


def make_distance(thumbs: np.ndarray, mask: np.ndarray) -> Callable[[int, int], float]:
    n = max(int(mask.sum()), 1)

    def dist(i: int, j: int) -> float:
        """Fraction of static pixels that differ between frames i and j."""
        return float(((np.abs(thumbs[i] - thumbs[j]) > PIXEL_DELTA) & mask).sum()) / n

    return dist


def select_stable(
    frames: list[Frame],
    dist: Callable[[int, int], float],
    *,
    same_threshold: float = 0.006,
    dup_threshold: float = 0.003,
    min_samples: int = 2,
) -> list[Frame]:
    """Group consecutive similar frames; keep the LAST frame of each segment.

    The last frame of a stable run holds the most complete state (fully typed
    code, fully revealed slide). Segments shorter than ``min_samples`` are
    treated as transitions. Representatives identical to an earlier kept frame
    (e.g. returning to a slide) are dropped.
    """
    if not frames:
        return []
    segments: list[list[int]] = [[0]]
    for i in range(1, len(frames)):
        if dist(i, segments[-1][-1]) <= same_threshold:
            segments[-1].append(i)
        else:
            segments.append([i])

    kept: list[int] = []
    for seg in segments:
        if len(seg) < min_samples:
            continue
        rep = seg[-1]
        if any(dist(rep, k) <= dup_threshold for k in kept):
            continue
        kept.append(rep)
    return [frames[k] for k in kept]


def candidate_frames(
    video: Path, out_dir: Path, *, interval: float, offset: float, min_samples: int
) -> tuple[list[Frame], int]:
    frames = sample(video, out_dir, interval, offset)
    thumbs = load_thumbs(frames)
    dist = make_distance(thumbs, motion_mask(thumbs))
    return select_stable(frames, dist, min_samples=min_samples), len(frames)
