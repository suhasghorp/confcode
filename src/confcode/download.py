"""Stage 1: fetch video, metadata and captions with yt-dlp."""

from __future__ import annotations

import html
import re
from pathlib import Path

import yt_dlp
from yt_dlp.utils import download_range_func

from .models import TranscriptSegment, VideoInfo


def fetch(url: str, work: Path, start: float | None, end: float | None) -> VideoInfo:
    work.mkdir(parents=True, exist_ok=True)
    opts: dict = {
        "format": "bv*[height<=1080][ext=mp4]+ba[ext=m4a]/bv*[height<=1080]+ba/b[height<=1080]/b",
        "merge_output_format": "mp4",
        "outtmpl": str(work / "video.%(ext)s"),
        "writesubtitles": True,
        "writeautomaticsub": True,
        "subtitleslangs": ["en", "en-orig", "en-US", "en-GB"],
        "subtitlesformat": "vtt",
        "quiet": True,
        "no_warnings": True,
        "noprogress": True,
    }
    if start is not None or end is not None:
        opts["download_ranges"] = download_range_func(
            None, [(start or 0, end if end is not None else float("inf"))]
        )
    with yt_dlp.YoutubeDL(opts) as ydl:
        info = ydl.extract_info(url, download=True)

    video = next((p for p in work.glob("video.*") if p.suffix in (".mp4", ".mkv", ".webm")), None)
    if video is None:
        raise RuntimeError("yt-dlp did not produce a video file")
    duration = float(info.get("duration") or 0)
    return VideoInfo(
        url=info.get("webpage_url") or url,
        video_id=info["id"],
        title=info.get("title", ""),
        channel=info.get("channel") or info.get("uploader") or "",
        upload_date=info.get("upload_date") or "",
        description=info.get("description") or "",
        duration=duration,
        chapters=info.get("chapters") or [],
        start=start or 0,
        end=end if end is not None else duration,
        video_path=str(video),
    )


def find_vtt(work: Path) -> Path | None:
    vtts = sorted(work.glob("video*.vtt"))
    # Prefer human subtitles (plain "en") over auto-generated variants.
    for pref in (".en.vtt", ".en-US.vtt", ".en-GB.vtt", ".en-orig.vtt"):
        for v in vtts:
            if v.name.endswith(pref):
                return v
    return vtts[0] if vtts else None


_TS = re.compile(r"(\d+):(\d\d):(\d\d)\.(\d+)|(\d+):(\d\d)\.(\d+)")
_TAG = re.compile(r"<[^>]+>")


def _secs(s: str) -> float:
    m = _TS.fullmatch(s.strip())
    if not m:
        raise ValueError(s)
    if m.group(1):
        h, mi, se, ms = m.group(1, 2, 3, 4)
    else:
        h, (mi, se, ms) = "0", m.group(5, 6, 7)
    return int(h) * 3600 + int(mi) * 60 + int(se) + int(ms) / 1000


def parse_vtt(text: str) -> list[TranscriptSegment]:
    """Parse WebVTT, collapsing YouTube auto-caption 'rolling' duplicate lines."""
    segs: list[TranscriptSegment] = []
    last_line = ""
    for block in re.split(r"\n\s*\n", text.replace("\r", "")):
        lines = block.strip().split("\n")
        ti = next((i for i, l in enumerate(lines) if "-->" in l), None)
        if ti is None:
            continue
        a, b = lines[ti].split("-->")
        start, end = _secs(a), _secs(b.split()[0])
        for raw in lines[ti + 1 :]:
            line = html.unescape(_TAG.sub("", raw)).strip()
            if not line or line == last_line:
                continue
            last_line = line
            segs.append(TranscriptSegment(start=start, end=end, text=line))
    return segs


def load_transcript(info: VideoInfo, work: Path) -> list[TranscriptSegment]:
    vtt = find_vtt(work)
    if vtt is not None:
        segs = parse_vtt(vtt.read_text(encoding="utf-8"))
    else:
        segs = _whisper(info)
    return [s for s in segs if s.end >= info.start and s.start <= info.end]


def _whisper(info: VideoInfo) -> list[TranscriptSegment]:
    try:
        from faster_whisper import WhisperModel  # type: ignore
    except ImportError:
        print("! No captions and faster-whisper not installed; continuing without transcript.")
        return []
    model = WhisperModel("small", device="auto")
    out, _ = model.transcribe(info.video_path, vad_filter=True)
    # Section downloads start at 0, so shift to absolute time.
    return [
        TranscriptSegment(start=s.start + info.start, end=s.end + info.start, text=s.text.strip())
        for s in out
    ]
