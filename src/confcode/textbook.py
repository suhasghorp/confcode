"""Stage 5 (textbook style): outline the chapter, then write each section in depth."""

from __future__ import annotations

import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from . import llm
from .models import (ChapterOutline, Frame, FrameReading, SectionDraft, Snippet,
                     TranscriptSegment, VideoInfo)
from .vision import _mmss
from .writer import _chunk_transcript, _slide_timeline

AUDIENCE = """The reader is a student who knows the basics of the talk's programming \
language but may be new to the topic. Write like a well-crafted textbook chapter: build intuition \
first, define every term before using it, explain *why* and not only *what*, and use \
concrete examples."""

OUTLINE_SYSTEM = f"""You turn a recorded technical talk into the outline of a textbook chapter.
{AUDIENCE}

You get video metadata, a timestamped auto-caption transcript (technical terms are often \
misheard - correct them using slide text and code), a slide timeline, and code snippets \
extracted verbatim from the slides, each with an ID.

Keep the spirit of the talk: follow its argument and its order, and keep its thesis and \
opinions attributed to the speaker. Where a student needs background the talk assumes \
(e.g. threads, callbacks, coroutines, futures, lifetimes), add an early section for it \
and give it the start time of where it is first needed. Sections must cover the whole talk, \
in chronological order, with non-overlapping [start_seconds, end_seconds) ranges as far as \
possible. Assign every snippet ID to exactly one section. The user message states how \
many sections to aim for. \
Exercises should test understanding of the talk's ideas (5-8 of them)."""

SECTION_SYSTEM = f"""You write ONE section of a textbook chapter based on a recorded talk.
{AUDIENCE}

Write detailed, introductory, well-structured Markdown (use ### subheadings, short \
paragraphs, lists and tables where helpful, and blockquotes for key insights). Explain \
concepts in your own words, go beyond the transcript where it helps learning, but keep \
the talk's argument, its examples, and its conclusions; when stating the speaker's \
opinion, attribute it ("the speaker argues...").

Code rules - these are strict:
- NEVER write fenced code blocks in body_md.
- Talk code: place each snippet listed for this section with a marker line \
{{{{snippet:ID}}}} where it fits, and walk through it line by line afterwards.
- Your own code: add 1-4 illustrative examples in `examples` (ids E1, E2, ...), each placed \
with a marker line {{{{example:ID}}}}. Examples should be small, complete, idiomatic and correct \
for the talk's language and its modern standard (for C++: C++20/C++26, and say when \
something comes from a library rather than the standard, e.g. stdexec's exec:: namespace). Use them for contrast (e.g. the callback or thread-based \
version of the same idea), for minimal building blocks, or for common pitfalls.
- Never present your own examples as the speaker's code.

Do not repeat the section heading, and do not write an introduction or summary for the \
whole chapter - other sections cover that."""


def _window(segs: list[TranscriptSegment], a: float, b: float, pad: float = 20) -> list[TranscriptSegment]:
    return [s for s in segs if s.end >= a - pad and s.start <= b + pad]


def _snips_xml(snips: list[Snippet]) -> str:
    return "\n\n".join(
        f"<snippet id=\"{s.id}\" at=\"{_mmss(s.first_ts)}\" language=\"{s.language}\""
        f" slide=\"{s.slide_title}\">\n{s.code}\n</snippet>" for s in snips
    ) or "(none)"


def outline(info: VideoInfo, transcript, readings, frames, snippets, *, model, effort, result_path: Path):
    minutes = max((info.end - info.start) / 60, 1)
    n_sections = max(6, min(18, round(minutes / 4) + 2))
    prompt = f"""Aim for about {n_sections} sections.

<video>
title: {info.title}
channel: {info.channel}
range: {_mmss(info.start)} - {_mmss(info.end)}
description:
{info.description[:3000]}
</video>

<slides>
{_slide_timeline(readings, frames) or "(none)"}
</slides>

<transcript>
{_chunk_transcript(transcript) or "(none)"}
</transcript>

<snippets>
{_snips_xml(snippets)}
</snippets>"""
    return llm.parse(schema=ChapterOutline, content=prompt, system=OUTLINE_SYSTEM, model=model,
                     effort=effort, max_tokens=32000, task=("outline", result_path))


def write_sections(
    out: ChapterOutline, info: VideoInfo, transcript, readings: dict[int, FrameReading],
    frames: list[Frame], snippets: list[Snippet], cache_dir: Path, *, model, effort, workers=4,
) -> list[SectionDraft]:
    cache_dir.mkdir(parents=True, exist_ok=True)
    by_id = {s.id: s for s in snippets}
    ts = {f.index: f.ts for f in frames}
    toc = "\n".join(
        f"{i + 1}. {s.heading} [{_mmss(s.start_seconds)}-{_mmss(s.end_seconds)}]: " + "; ".join(s.key_points)
        for i, s in enumerate(out.sections)
    )

    pending: list[Path] = []

    def run(i: int) -> SectionDraft | None:
        cache = cache_dir / f"section_{i:02d}.json"
        if cache.exists():
            return SectionDraft.model_validate(json.loads(cache.read_text()))
        sec = out.sections[i]
        a, b = sec.start_seconds, max(sec.end_seconds, sec.start_seconds + 30)
        local_readings = {k: r for k, r in readings.items() if a - 20 <= ts.get(k, -1) <= b + 20}
        prompt = f"""<chapter title="{out.title}">
<table_of_contents>
{toc}
</table_of_contents>
</chapter>

<this_section number="{i + 1}" heading="{sec.heading}">
key points from the talk:
{chr(10).join('- ' + p for p in sec.key_points)}
teaching plan: {sec.teaching_plan}
snippets to place: {', '.join(sec.snippet_ids) or '(none)'}
</this_section>

<slides_in_this_part>
{_slide_timeline(local_readings, frames) or "(none)"}
</slides_in_this_part>

<transcript_of_this_part>
{_chunk_transcript(_window(transcript, a, b)) or "(no talk content in this range - this is a background section; write it from your own knowledge)"}
</transcript_of_this_part>

<snippets>
{_snips_xml([by_id[x] for x in sec.snippet_ids if x in by_id])}
</snippets>

Write section {i + 1} now."""
        try:
            draft, usage = llm.parse(schema=SectionDraft, content=prompt, system=SECTION_SYSTEM,
                                     model=model, effort=effort, max_tokens=32000,
                                     task=(cache.stem, cache))
        except llm.PendingTasks as p:
            pending.extend(p.paths)
            return None
        cache.write_text(draft.model_dump_json(indent=1))
        print(f"      section {i + 1}/{len(out.sections)} '{sec.heading}': "
              f"{len(draft.body_md.split())} words, {len(draft.examples)} example(s), "
              f"{usage['input_tokens']} in / {usage['output_tokens']} out tokens")
        return draft

    with ThreadPoolExecutor(max_workers=workers) as pool:
        drafts = list(pool.map(run, range(len(out.sections))))
    if pending:
        raise llm.PendingTasks(sorted(pending))
    return drafts
