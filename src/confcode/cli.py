"""confcode CLI: conference talk on YouTube -> textbook-style Markdown article with all on-screen code."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Optional
from urllib.parse import parse_qs, urlparse

import typer

from . import download, frames as frames_mod, llm, merge, render, textbook, vision, writer
from .models import (ChapterOutline, Frame, FrameReading, PostPlan, SectionDraft, Snippet,
                     TranscriptSegment, VideoInfo)

app = typer.Typer(add_completion=False, help=__doc__)


def parse_time(s: Optional[str]) -> Optional[float]:
    if s is None:
        return None
    s = s.strip().lower().rstrip("s")
    if ":" in s:
        secs = 0.0
        for part in s.split(":"):
            secs = secs * 60 + float(part)
        return secs
    return float(s)


def _video_id(url: str) -> str:
    q = parse_qs(urlparse(url).query)
    if "v" in q:
        return q["v"][0]
    return re.sub(r"\W+", "_", urlparse(url).path.strip("/")) or "video"


def _save(path: Path, data) -> None:
    path.write_text(json.dumps(data, indent=1, ensure_ascii=False), encoding="utf-8")


def _load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


@app.command()
def main(
    url: str = typer.Argument(..., help="YouTube video URL"),
    out: Optional[Path] = typer.Option(None, help="Output directory (default: out/<video_id>[_range])"),
    start: Optional[str] = typer.Option(None, help="Start time (secs or mm:ss / hh:mm:ss)"),
    end: Optional[str] = typer.Option(None, help="End time (secs or mm:ss / hh:mm:ss)"),
    model: str = typer.Option(llm.DEFAULT_MODEL, help="Claude model for both stages"),
    vision_effort: str = typer.Option("low", help="Effort for frame reading"),
    style: str = typer.Option("textbook", help="textbook (detailed chapter) or blog (concise summary)"),
    writer_effort: str = typer.Option("high", help="Effort for writing the post"),
    interval: float = typer.Option(2.0, help="Seconds between sampled frames"),
    min_samples: int = typer.Option(2, help="Frames a screen must stay stable to count"),
    max_frames: int = typer.Option(250, help="Abort if more candidate frames than this"),
    batch_size: int = typer.Option(6, help="Frames per vision request"),
    save_frames: bool = typer.Option(True, help="Copy the frame behind each snippet into out/frames"),
    dry_run: bool = typer.Option(False, help="Stop after frame selection; no Claude calls"),
    redo: list[str] = typer.Option([], help="Force re-run of a stage: frames, vision, merge, write"),
    backend: str = typer.Option("api", help="api (call Claude API) or session (write tasks to "
                                "<out>/.work/session/ for a Claude Code session to fill; textbook only)"),
) -> None:
    try:
        _run(url, out, start, end, model, vision_effort, style, writer_effort, interval, min_samples,
             max_frames, batch_size, save_frames, dry_run, redo, backend)
    except llm.PendingTasks as p:
        print(f"\nSession backend: {len(p.paths)} task(s) waiting. For each, follow the task file and "
              "write the result JSON it names, then rerun the same command:")
        for t in p.paths:
            print(f"  {t}")
        raise typer.Exit(3)


def _run(url, out, start, end, model, vision_effort, style, writer_effort, interval, min_samples,
         max_frames, batch_size, save_frames, dry_run, redo, backend) -> None:
    s, e = parse_time(start), parse_time(end)
    if out is None:
        suffix = "" if s is None and e is None else f"_{int(s or 0)}-{int(e) if e is not None else 'end'}"
        out = Path("out") / f"{_video_id(url)}{suffix}"
    work = out / ".work"
    work.mkdir(parents=True, exist_ok=True)
    redo_set = set(redo)
    if backend == "session":
        if style != "textbook":
            raise typer.BadParameter("--backend session supports --style textbook only")
        llm.SESSION_DIR = work / "session"
    elif backend != "api":
        raise typer.BadParameter("--backend must be 'api' or 'session'")

    # 1. fetch
    info_p, tr_p = work / "info.json", work / "transcript.json"
    if info_p.exists() and Path(VideoInfo.model_validate(_load(info_p)).video_path).exists():
        info = VideoInfo.model_validate(_load(info_p))
        print(f"[1/6] fetch: cached ({info.title})")
    else:
        print("[1/6] fetch: downloading video + captions ...")
        info = download.fetch(url, work, s, e)
        _save(info_p, info.model_dump())
    if tr_p.exists():
        transcript = [TranscriptSegment.model_validate(x) for x in _load(tr_p)]
    else:
        transcript = download.load_transcript(info, work)
        _save(tr_p, [t.model_dump() for t in transcript])
    print(f"      {info.title} | {info.channel} | range {info.start:.0f}-{info.end:.0f}s "
          f"| {len(transcript)} caption lines")

    # 2. frames
    fr_p = work / "frames.json"
    if fr_p.exists() and "frames" not in redo_set:
        cands = [Frame.model_validate(x) for x in _load(fr_p)]
        print(f"[2/6] frames: cached ({len(cands)} candidates)")
    else:
        print("[2/6] frames: sampling + selecting stable screens ...")
        cands, total = frames_mod.candidate_frames(
            Path(info.video_path), work / "frames", interval=interval, offset=info.start,
            min_samples=min_samples,
        )
        _save(fr_p, [f.model_dump() for f in cands])
        print(f"      {total} sampled -> {len(cands)} candidate frames")
        redo_set |= {"vision", "merge", "write"}
    n_calls = -(-len(cands) // batch_size)
    if dry_run:
        print(f"Dry run: {len(cands)} frames -> {n_calls} vision request(s) + 1 writer request.")
        print(f"Inspect candidates in {work / 'frames'} (see frames.json for which were kept).")
        return
    if len(cands) > max_frames:
        raise typer.Exit(f"{len(cands)} candidate frames > --max-frames {max_frames}. "
                         "Raise the cap, increase --interval, or narrow --start/--end.")

    # 3. vision
    vis_p = work / "readings.json"
    if vis_p.exists() and "vision" not in redo_set:
        readings = {int(k): FrameReading.model_validate(v) for k, v in _load(vis_p).items()}
        print(f"[3/6] vision: cached ({len(readings)} frames)")
    else:
        print(f"[3/6] vision: reading {len(cands)} frames in {n_calls} request(s) with {model} ...")
        cache_dir = work / "vision"
        if "vision" in redo_set and cache_dir.exists():
            for f in cache_dir.glob("*.json"):
                f.unlink()
        readings = vision.read_frames(cands, cache_dir, model=model, effort=vision_effort,
                                      batch_size=batch_size)
        _save(vis_p, {k: v.model_dump() for k, v in readings.items()})
        redo_set |= {"merge", "write"}

    # 4. merge
    sn_p = work / "snippets.json"
    if sn_p.exists() and "merge" not in redo_set:
        snippets = [Snippet.model_validate(x) for x in _load(sn_p)]
        print(f"[4/6] merge: cached ({len(snippets)} snippets)")
    else:
        raw = sum(len(r.code_blocks) for r in readings.values())
        snippets = merge.merge(readings, cands)
        _save(sn_p, [x.model_dump() for x in snippets])
        print(f"[4/6] merge: {raw} on-screen code views -> {len(snippets)} distinct snippets")
        redo_set.add("write")

    # 5-6. write + render
    # The session backend's write stage is hand-made work: an upstream redo never
    # discards it implicitly, only an explicit --redo write does.
    if backend == "session" and "write" not in set(redo):
        redo_set.discard("write")

    if style == "blog":
        plan_p = work / "plan.json"
        if plan_p.exists() and "write" not in redo_set:
            plan = PostPlan.model_validate(_load(plan_p))
            print("[5/6] write: cached")
        else:
            print(f"[5/6] write: drafting post with {model} ...")
            plan, usage = writer.plan_post(info, transcript, readings, cands, snippets,
                                           model=model, effort=writer_effort)
            _save(plan_p, plan.model_dump())
            print(f"      {len(plan.sections)} sections, {usage['input_tokens']} in / "
                  f"{usage['output_tokens']} out tokens")
        post = render.render(info, plan, snippets, out, save_frames=save_frames)
    elif style == "textbook":
        ol_p, sec_dir = work / "outline.json", work / "sections"
        if "write" in redo_set:
            ol_p.unlink(missing_ok=True)
            for f in sec_dir.glob("*.json") if sec_dir.exists() else []:
                f.unlink()
        if ol_p.exists():
            outline = ChapterOutline.model_validate(_load(ol_p))
            print(f"[5/6] write: outline cached ({len(outline.sections)} sections)")
        else:
            print(f"[5/6] write: outlining chapter with {model} ...")
            outline, usage = textbook.outline(info, transcript, readings, cands, snippets,
                                              model=model, effort=writer_effort, result_path=ol_p)
            _save(ol_p, outline.model_dump())
            print(f"      {len(outline.sections)} sections, {usage['input_tokens']} in / "
                  f"{usage['output_tokens']} out tokens")
        drafts: list[SectionDraft] = textbook.write_sections(
            outline, info, transcript, readings, cands, snippets, sec_dir,
            model=model, effort=writer_effort)
        post = render.render_textbook(info, outline, drafts, snippets, out, save_frames=save_frames)
    else:
        raise typer.BadParameter("--style must be 'textbook' or 'blog'")
    words = len(post.read_text().split())
    print(f"[6/6] render: {post}  ({words} words, {len(snippets)} talk snippets in {out / 'code'})")


if __name__ == "__main__":
    app()
