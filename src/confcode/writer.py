"""Stage 5: have Claude organize topics and prose around the extracted snippets."""

from __future__ import annotations

from . import llm
from .models import FrameReading, Frame, PostPlan, Snippet, TranscriptSegment, VideoInfo
from .vision import _mmss

SYSTEM = """You turn a recorded programming talk or tutorial into a technical blog post.

You get: video metadata, a timestamped transcript (auto-captions: technical terms are \
often misheard - correct them using the slide text and code), a timeline of on-screen \
slides, and code snippets extracted verbatim from the screen, each with an ID.

Produce a post plan:
- title: a blog-style title (may differ from the video title).
- summary: 2-4 sentence overview of the talk's argument.
- tags: 3-8 lowercase tags.
- sections: one per topic actually discussed, in chronological order. start_seconds is \
when the topic starts (absolute video time). body_md is well-written Markdown prose \
explaining the topic as the speaker did: the ideas, the reasoning, the trade-offs, \
and what each attached snippet demonstrates. Write in a neutral blog voice, not \
"the speaker says".
- snippet_ids: attach each snippet to the section where it is discussed, in order. \
Every snippet ID should be used exactly once.

NEVER write code blocks or retype code in body_md - code is inserted from snippet_ids \
verbatim. Inline `identifiers` are fine."""


def _chunk_transcript(segs: list[TranscriptSegment], window: float = 30) -> str:
    out, buf, t0 = [], [], None
    for s in segs:
        if t0 is None:
            t0 = s.start
        buf.append(s.text)
        if s.start - t0 >= window:
            out.append(f"[{_mmss(t0)}] {' '.join(buf)}")
            buf, t0 = [], None
    if buf and t0 is not None:
        out.append(f"[{_mmss(t0)}] {' '.join(buf)}")
    return "\n".join(out)


def _slide_timeline(readings: dict[int, FrameReading], frames: list[Frame]) -> str:
    ts = {f.index: f.ts for f in frames}
    lines, last = [], None
    for idx in sorted(readings):
        r = readings[idx]
        key = (r.slide_title, r.slide_text)
        if key == last or not (r.slide_title or r.slide_text):
            continue
        last = key
        text = f" - {r.slide_text}" if r.slide_text else ""
        lines.append(f"[{_mmss(ts.get(idx, 0))}] {r.slide_title}{text}")
    return "\n".join(lines)


def plan_post(
    info: VideoInfo, transcript: list[TranscriptSegment], readings: dict[int, FrameReading],
    frames: list[Frame], snippets: list[Snippet], *, model: str, effort: str,
) -> tuple[PostPlan, dict]:
    chapters = "\n".join(f"[{_mmss(c['start_time'])}] {c['title']}" for c in info.chapters) or "(none)"
    snips = "\n\n".join(
        f"<snippet id=\"{s.id}\" at=\"{_mmss(s.first_ts)}\" language=\"{s.language}\""
        f" file=\"{s.filename_hint}\" slide=\"{s.slide_title}\">\n{s.code}\n</snippet>"
        for s in snippets
    ) or "(no code was shown)"
    prompt = f"""<video>
title: {info.title}
channel: {info.channel}
processed range: {_mmss(info.start)} - {_mmss(info.end)}
description:
{info.description[:3000]}
</video>

<chapters>
{chapters}
</chapters>

<slides>
{_slide_timeline(readings, frames) or "(none)"}
</slides>

<transcript>
{_chunk_transcript(transcript) or "(no transcript available)"}
</transcript>

<snippets>
{snips}
</snippets>"""
    return llm.parse(schema=PostPlan, content=prompt, system=SYSTEM, model=model,
                     effort=effort, max_tokens=32000)
