import numpy as np

from confcode.frames import make_distance, motion_mask, select_stable
from confcode.models import Frame


def _frames(n):
    return [Frame(index=i, ts=float(i * 2), path=f"{i}.jpg") for i in range(n)]


def _dist_from_labels(labels):
    return lambda i, j: 0.0 if labels[i] == labels[j] else 1.0


def test_keeps_last_frame_of_each_stable_run():
    kept = select_stable(_frames(6), _dist_from_labels("aaabbb"))
    assert [f.index for f in kept] == [2, 5]


def test_drops_transitions_and_revisits():
    kept = select_stable(_frames(7), _dist_from_labels("aatbbaa"))
    assert [f.index for f in kept] == [1, 4]


def test_moving_speaker_region_is_ignored():
    rng = np.random.default_rng(0)
    slide_a = np.full((108, 192), 240, dtype=np.int16)
    slide_b = slide_a.copy()
    slide_b[40:70, 100:180] = 0  # new code block appears
    thumbs = []
    for k in range(40):
        f = (slide_a if k < 20 else slide_b).copy()
        f[:, :40] = rng.integers(0, 255, (108, 40))  # speaker camera noise
        thumbs.append(f)
    thumbs = np.stack(thumbs)
    mask = motion_mask(thumbs)
    assert not mask[:, :35].any() and mask[:, 60:].all()
    kept = select_stable(_frames(40), make_distance(thumbs, mask))
    assert [f.index for f in kept] == [19, 39]
