"""Stage 3: read slide text and on-screen code from frames with Claude vision."""

from __future__ import annotations

import base64
import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from . import llm
from .models import BatchReading, Frame, FrameReading

SYSTEM = """You transcribe screenshots from programming videos (conference talks, \
tutorials, live coding) for a blog post.

For every frame you are given, return one entry with its frame_index and:
- slide_title: the heading of the slide/screen, or "".
- slide_text: short condensed version of non-code text (bullets, diagram labels), or "".
- code_blocks: every distinct block of source code, shell command, or program output on screen.

Rules for code:
- Transcribe EXACTLY what is visible, character for character. Preserve indentation \
(use spaces), blank lines, punctuation, templates (<>), scope operators (::), \
operators, and comments. Never fix, complete, modernize, or reformat code.
- Omit editor chrome: line numbers, gutter markers, tab bars, minimap, cursor, \
squiggles, and shell prompts like "$ " or "PS>" (keep the command itself).
- If text is unreadable, write your best reading and put "/* ? */" where unsure.
- language: lowercase fence id (cpp, c, python, rust, go, java, javascript, \
typescript, csharp, bash, console for terminal output, text if unsure).
- If two code panels are shown side by side (e.g. before/after), return them as separate blocks.
- is_partial = true if the code visibly continues beyond the screen or is mid-typing.
- Frames with no code (title slides, speaker only, audience) get code_blocks: [].
- Speaker video overlays, logos and conference branding are not content; ignore them."""


def _img_block(path: str) -> dict:
    if llm.SESSION_DIR is not None:  # session backend: point at the file instead of embedding it
        return {"type": "text", "text": f"[image file: {Path(path).resolve()}]"}
    data = base64.standard_b64encode(Path(path).read_bytes()).decode()
    return {"type": "image", "source": {"type": "base64", "media_type": "image/jpeg", "data": data}}


def _mmss(t: float) -> str:
    t = int(t)
    return f"{t // 3600}:{t % 3600 // 60:02d}:{t % 60:02d}" if t >= 3600 else f"{t // 60}:{t % 60:02d}"


def _read_batch(batch: list[Frame], model: str, effort: str, cache: Path) -> tuple[list[FrameReading], dict]:
    content: list[dict] = []
    for f in batch:
        content.append({"type": "text", "text": f"Frame {f.index} (at {_mmss(f.ts)}):"})
        content.append(_img_block(f.path))
    content.append(
        {"type": "text", "text": f"Return exactly {len(batch)} entries, one per frame index above."}
    )
    out, usage = llm.parse(
        schema=BatchReading, content=content, system=SYSTEM, model=model, effort=effort,
        max_tokens=32000, task=(cache.stem, cache),
    )
    wanted = {f.index for f in batch}
    return [r for r in out.frames if r.frame_index in wanted], usage


def read_frames(
    frames: list[Frame], cache_dir: Path, *, model: str, effort: str,
    batch_size: int = 6, workers: int = 4,
) -> dict[int, FrameReading]:
    """Returns frame_index -> reading. Each batch is cached on disk."""
    cache_dir.mkdir(parents=True, exist_ok=True)
    batches = [frames[i : i + batch_size] for i in range(0, len(frames), batch_size)]

    pending: list[Path] = []

    def run(batch: list[Frame]) -> list[FrameReading]:
        key = "-".join(str(f.index) for f in batch)
        cache = cache_dir / f"batch_{key}.json"
        if cache.exists():
            return [FrameReading.model_validate(r) for r in json.loads(cache.read_text())["frames"]]
        try:
            readings, usage = _read_batch(batch, model, effort, cache)
        except llm.PendingTasks as p:
            pending.extend(p.paths)
            return []
        cache.write_text(json.dumps({"frames": [r.model_dump() for r in readings], "usage": usage}, indent=1))
        n_code = sum(len(r.code_blocks) for r in readings)
        print(f"  frames {batch[0].index}-{batch[-1].index}: {n_code} code block(s), "
              f"{usage['input_tokens']} in / {usage['output_tokens']} out tokens")
        return readings

    result: dict[int, FrameReading] = {}
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for readings in pool.map(run, batches):
            for r in readings:
                result[r.frame_index] = r
    if pending:
        raise llm.PendingTasks(sorted(pending))
    return result
