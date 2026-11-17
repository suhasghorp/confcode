"""Stage 4: collapse the many on-screen views of code into distinct snippets.

Deterministic, no LLM. Handles:
- the same code seen again          -> one snippet (extend last_ts)
- code being typed / revealed       -> later superset replaces earlier version
- a partial view (highlight, zoom)  -> dropped when already contained
- scrolling                         -> stitched on overlapping lines
Distinct versions (e.g. a "before" and "after" refactoring) stay separate.
"""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass

from .models import FrameReading, Frame, Snippet

MIN_STITCH_OVERLAP = 3


@dataclass
class _Obs:
    ts: float
    frame_path: str
    language: str
    filename_hint: str
    code: str
    slide_title: str


def normalize(code: str) -> str:
    lines = [l.rstrip() for l in code.expandtabs(4).split("\n")]
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    # Remove common leading indentation.
    ind = min((len(l) - len(l.lstrip()) for l in lines if l.strip()), default=0)
    return "\n".join(l[ind:] for l in lines)


def _keys(code: str) -> list[str]:
    """Whitespace-insensitive line keys, used for comparing versions."""
    return ["".join(l.split()) for l in code.split("\n") if l.strip()]


def contains(big: str, small: str, *, allow_unfinished_last: bool = False) -> bool:
    """True if every non-empty line of small also appears in big.

    With allow_unfinished_last, small's final line may instead be a prefix of
    a line in big (it was caught mid-typing). Exact line matching (ignoring
    whitespace) keeps distinct versions such as before/after slides apart.
    """
    s = _keys(small)
    if not s:
        return True
    b = Counter(_keys(big))
    missing = []
    for k in s:
        if b[k] > 0:
            b[k] -= 1
        else:
            missing.append(k)
    if not missing:
        return True
    if not (allow_unfinished_last and missing == [s[-1]]):
        return False
    return any(k.startswith(s[-1]) for k in b if b[k] > 0)


def stitch(top: str, bottom: str, min_overlap: int = MIN_STITCH_OVERLAP) -> str | None:
    """Join two scrolled views if bottom's head overlaps top's tail."""
    t_lines, b_lines = top.split("\n"), bottom.split("\n")
    tk, bk = _keys(top), _keys(bottom)
    if len(tk) < min_overlap or len(bk) < min_overlap:
        return None
    for n in range(min(len(tk), len(bk)) - 1, min_overlap - 1, -1):
        if tk[-n:] == bk[:n]:
            # Find where the n-th non-empty line of bottom ends.
            seen, cut = 0, len(b_lines)
            for i, l in enumerate(b_lines):
                if l.strip():
                    seen += 1
                    if seen == n:
                        cut = i + 1
                        break
            rest = b_lines[cut:]
            return "\n".join(t_lines + rest) if rest else None
    return None


def _compatible(a: Snippet, o: _Obs) -> bool:
    if a.filename_hint and o.filename_hint and a.filename_hint != o.filename_hint:
        return False
    generic = {"text", ""}
    return a.language == o.language or a.language in generic or o.language in generic


def merge(readings: dict[int, FrameReading], frames: list[Frame]) -> list[Snippet]:
    by_index = {f.index: f for f in frames}
    obs: list[_Obs] = []
    for idx in sorted(readings):
        r, f = readings[idx], by_index.get(idx)
        if f is None:
            continue
        for b in r.code_blocks:
            code = normalize(b.code)
            if len(_keys(code)) == 0:
                continue
            obs.append(_Obs(f.ts, f.path, b.language.lower().strip(), b.filename_hint.strip(),
                            code, r.slide_title.strip()))

    snippets: list[Snippet] = []
    for o in obs:
        placed = False
        # Search recent snippets first: on-screen code evolves locally in time.
        for s in reversed(snippets):
            if not _compatible(s, o):
                continue
            if contains(s.code, o.code):
                s.last_ts = o.ts  # already have it (or a superset)
                placed = True
            elif contains(o.code, s.code, allow_unfinished_last=True):
                s.code, s.frame_path, s.last_ts = o.code, o.frame_path, o.ts  # grew
                if o.filename_hint:
                    s.filename_hint = o.filename_hint
                if s.language in ("", "text"):
                    s.language = o.language
                placed = True
            elif (joined := stitch(s.code, o.code)) is not None:
                s.code, s.last_ts = joined, o.ts  # scrolled down
                placed = True
            if placed:
                break
        if not placed:
            snippets.append(Snippet(
                id="", language=o.language or "text", filename_hint=o.filename_hint, code=o.code,
                first_ts=o.ts, last_ts=o.ts, frame_path=o.frame_path, slide_title=o.slide_title,
            ))

    # A snippet that grew may now contain an earlier separate one; drop those.
    final: list[Snippet] = []
    for s in snippets:
        if any(t is not s and len(_keys(t.code)) > len(_keys(s.code)) and contains(t.code, s.code)
               and t.language == s.language for t in snippets):
            continue
        final.append(s)
    final.sort(key=lambda s: s.first_ts)
    for i, s in enumerate(final, 1):
        s.id = f"S{i:02d}"
    return final
