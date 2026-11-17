"""Stage 6: render the final Markdown post and per-snippet code files."""

from __future__ import annotations

import json
import re
import shutil
from datetime import date
from pathlib import Path

from .models import PostPlan, Snippet, VideoInfo
from .vision import _mmss

EXT = {
    "cpp": "cpp", "c++": "cpp", "c": "c", "python": "py", "rust": "rs", "go": "go",
    "java": "java", "javascript": "js", "typescript": "ts", "csharp": "cs", "bash": "sh",
    "shell": "sh", "sh": "sh", "console": "txt", "text": "txt", "kotlin": "kt", "swift": "swift",
    "ruby": "rb", "php": "php", "sql": "sql", "haskell": "hs", "zig": "zig", "cmake": "cmake",
    "json": "json", "yaml": "yaml", "toml": "toml", "html": "html", "css": "css",
}


def fence(code: str, lang: str) -> str:
    longest = max((len(m) for m in re.findall(r"`+", code)), default=0)
    ticks = "`" * max(3, longest + 1)
    return f"{ticks}{lang}\n{code}\n{ticks}"


def _yaml_str(s: str) -> str:
    return json.dumps(s, ensure_ascii=False)  # JSON strings are valid YAML scalars


def _ts_link(info: VideoInfo, t: float) -> str:
    return f"[{_mmss(t)}](https://www.youtube.com/watch?v={info.video_id}&t={int(t)}s)"


def snippet_filename(s: Snippet) -> str:
    base = Path(s.filename_hint).stem if s.filename_hint else (s.slide_title or s.language)
    slug = re.sub(r"[^a-z0-9]+", "_", base.lower()).strip("_")
    if len(slug) > 40:
        slug = slug[:40].rsplit("_", 1)[0]
    slug = slug or "snippet"
    return f"{s.id}_{slug}.{EXT.get(s.language, 'txt')}"


def _write_code_files(snippets: list[Snippet], out: Path, save_frames: bool) -> None:
    code_dir = out / "code"
    if code_dir.exists():
        shutil.rmtree(code_dir)
    code_dir.mkdir()
    for s in snippets:
        (code_dir / snippet_filename(s)).write_text(s.code + "\n", encoding="utf-8")
    if save_frames:
        fdir = out / "frames"
        fdir.mkdir(exist_ok=True)
        for s in snippets:
            if s.frame_path and Path(s.frame_path).exists():
                shutil.copy(s.frame_path, fdir / f"{s.id}.jpg")


def render(
    info: VideoInfo, plan: PostPlan, snippets: list[Snippet], out: Path, *, save_frames: bool,
) -> Path:
    out.mkdir(parents=True, exist_ok=True)
    by_id = {s.id: s for s in snippets}

    _write_code_files(snippets, out, save_frames)

    def block(s: Snippet) -> str:
        cap = f"*{s.filename_hint} — " if s.filename_hint else "*"
        cap += f"shown at {_ts_link(info, s.first_ts)}*"
        return f"{cap}\n\n{fence(s.code, s.language)}"

    used: set[str] = set()
    parts: list[str] = [
        "---",
        f"title: {_yaml_str(plan.title)}",
        f"date: {date.today().isoformat()}",
        f"tags: [{', '.join(_yaml_str(t) for t in plan.tags)}]",
        f"source: {_yaml_str(info.url)}",
        f"video_title: {_yaml_str(info.title)}",
        f"channel: {_yaml_str(info.channel)}",
        "---",
        "",
        f"> Based on [{info.title}]({info.url}) by {info.channel}.",
        "",
        plan.summary,
        "",
        "## Topics",
        "",
    ]
    for sec in plan.sections:
        parts.append(f"- {_ts_link(info, sec.start_seconds)} {sec.heading}")
    parts.append("")
    for sec in plan.sections:
        parts += [f"## {sec.heading}", "", sec.body_md.strip(), ""]
        for sid in sec.snippet_ids:
            if sid in by_id and sid not in used:
                used.add(sid)
                parts += [block(by_id[sid]), ""]
    leftover = [s for s in snippets if s.id not in used]
    if leftover:  # guarantee every on-screen snippet makes it into the post
        parts += ["## Additional code shown", ""]
        for s in leftover:
            parts += [block(s), ""]

    post = out / "post.md"
    post.write_text("\n".join(parts).rstrip() + "\n", encoding="utf-8")
    return post


# ---------- textbook style ----------

_MARKER = re.compile(r"^[ \t]*\{\{\s*(snippet|example)\s*:\s*([A-Za-z0-9_-]+)\s*\}\}[ \t]*$", re.M)


def _number_listings(outline, drafts, snippets: list[Snippet]) -> tuple[dict, dict]:
    """Simulate placement order -> (snippet id -> n, (section no, example id) -> n)."""
    sn: dict[str, int] = {}
    ex: dict[tuple[int, str], int] = {}
    known = {s.id for s in snippets}
    n = 0
    for i, (sec, draft) in enumerate(zip(outline.sections, drafts), 1):
        ex_ids = {e.id for e in draft.examples}
        for m in _MARKER.finditer(draft.body_md):
            kind, key = m.group(1), m.group(2)
            if kind == "snippet" and key in known and key not in sn:
                n += 1; sn[key] = n
            elif kind == "example" and key in ex_ids and (i, key) not in ex:
                n += 1; ex[(i, key)] = n
        for sid in sec.snippet_ids:
            if sid in known and sid not in sn:
                n += 1; sn[sid] = n
        for e in draft.examples:
            if (i, e.id) not in ex:
                n += 1; ex[(i, e.id)] = n
    for s in snippets:
        if s.id not in sn:
            n += 1; sn[s.id] = n
    return sn, ex


_ID_REF = re.compile(r"\b(?:[Ee]xample |[Ss]nippet |[Ll]isting )?(S\d{2}|E\d{1,2})\b")


def _resolve_refs(text: str, sec_no: int, sn: dict, ex: dict) -> str:
    """Replace internal IDs (S03, Example E1) with the reader-visible 'Listing N'."""
    def rep(m: re.Match) -> str:
        key = m.group(1)
        n = sn.get(key) if key.startswith("S") else ex.get((sec_no, key))
        return f"Listing {n}" if n else m.group(0)

    # Leave {{snippet:..}} / {{example:..}} markers alone.
    return "".join(
        part if _MARKER.fullmatch(part.strip() or "x") else _ID_REF.sub(rep, part)
        for part in re.split(r"(^[ \t]*\{\{[^}]*\}\}[ \t]*$)", text, flags=re.M)
    )


def render_textbook(info, outline, drafts, snippets: list[Snippet], out: Path, *, save_frames: bool) -> Path:
    out.mkdir(parents=True, exist_ok=True)
    sn_no, ex_no = _number_listings(outline, drafts, snippets)
    drafts = [
        d.model_copy(update={
            "body_md": _resolve_refs(d.body_md, i, sn_no, ex_no),
            "examples": [e.model_copy(update={"code": _resolve_refs(e.code, i, sn_no, ex_no)})
                         for e in d.examples],
        })
        for i, d in enumerate(drafts, 1)
    ]
    fix = lambda t: _resolve_refs(t, 0, sn_no, {})  # chapter-level text: talk snippets only
    outline = outline.model_copy(update={
        "introduction": fix(outline.introduction),
        "key_takeaways": [fix(k) for k in outline.key_takeaways],
        "glossary": [g.model_copy(update={"definition": fix(g.definition)}) for g in outline.glossary],
        "exercises": [x.model_copy(update={"question": fix(x.question), "hint": fix(x.hint)})
                      for x in outline.exercises],
    })
    by_id = {s.id: s for s in snippets}
    _write_code_files(snippets, out, save_frames)
    ex_dir = out / "code" / "examples"
    ex_dir.mkdir(parents=True, exist_ok=True)

    used: set[str] = set()
    listing = 0  # global numbering of all code listings

    def talk_block(s: Snippet) -> str:
        nonlocal listing
        listing += 1
        assert listing == sn_no[s.id], "listing numbering out of sync"
        where = f" ({s.slide_title})" if s.slide_title else ""
        return (f"**Listing {listing}** — code from the talk{where}, shown at {_ts_link(info, s.first_ts)}\n\n"
                f"{fence(s.code, s.language)}")

    def example_block(sec_no: int, e) -> str:
        nonlocal listing
        listing += 1
        assert listing == ex_no[(sec_no, e.id)], "listing numbering out of sync"
        name = f"{sec_no:02d}_{e.id}_{re.sub(r'[^a-z0-9]+', '_', e.title.lower()).strip('_')[:40]}"
        (ex_dir / f"{name}.{EXT.get(e.language.lower(), 'txt')}").write_text(e.code + "\n", encoding="utf-8")
        return (f"**Listing {listing}** — {e.title} *(illustrative example, not from the talk)*\n\n"
                f"{fence(e.code, e.language.lower())}")

    parts: list[str] = [
        "---",
        f"title: {_yaml_str(outline.title)}",
        f"date: {date.today().isoformat()}",
        f"tags: [{', '.join(_yaml_str(t) for t in outline.tags)}]",
        f"source: {_yaml_str(info.url)}",
        f"video_title: {_yaml_str(info.title)}",
        f"channel: {_yaml_str(info.channel)}",
        "---",
        "",
        f"> This chapter is based on the talk [{info.title}]({info.url}) ({info.channel}). "
        "Code listings marked *from the talk* are transcribed from the slides; listings marked "
        "*illustrative* were written for this chapter to explain the ideas.",
        "",
        outline.introduction.strip(),
        "",
        "## What you will learn",
        "",
        *[f"- {o}" for o in outline.learning_objectives],
        "",
        "## Before you start",
        "",
        *[f"- {p}" for p in outline.prerequisites],
        "",
        "## Contents",
        "",
        *[f"{i}. [{s.heading}](#{_anchor(i, s.heading)}) — {_ts_link(info, s.start_seconds)}"
          for i, s in enumerate(outline.sections, 1)],
        "",
    ]
    for i, (sec, draft) in enumerate(zip(outline.sections, drafts), 1):
        examples = {e.id: e for e in draft.examples}
        placed_ex: set[str] = set()

        def sub(m: re.Match) -> str:
            kind, key = m.group(1), m.group(2)
            if kind == "snippet" and key in by_id and key not in used:
                used.add(key)
                return talk_block(by_id[key])
            if kind == "example" and key in examples and key not in placed_ex:
                placed_ex.add(key)
                return example_block(i, examples[key])
            return ""

        body = _MARKER.sub(sub, draft.body_md.strip())
        parts += [f"## {i}. {sec.heading}", "", f"*Talk segment: {_ts_link(info, sec.start_seconds)}*", "", body, ""]
        # Anything assigned to the section but not placed by the writer goes at the end.
        for sid in sec.snippet_ids:
            if sid in by_id and sid not in used:
                used.add(sid)
                parts += [talk_block(by_id[sid]), ""]
        for e in draft.examples:
            if e.id not in placed_ex:
                parts += [example_block(i, e), ""]

    leftover = [s for s in snippets if s.id not in used]
    if leftover:
        parts += ["## Appendix: other code shown in the talk", ""]
        for s in leftover:
            used.add(s.id)
            parts += [talk_block(s), ""]

    parts += ["## Key takeaways", "", *[f"- {k}" for k in outline.key_takeaways], ""]
    if outline.glossary:
        parts += ["## Glossary", "", "| Term | Meaning |", "|---|---|",
                  *[f"| {g.term} | {g.definition.replace('|', '\\|')} |" for g in outline.glossary], ""]
    if outline.exercises:
        parts += ["## Exercises", ""]
        for n, ex in enumerate(outline.exercises, 1):
            parts += [f"{n}. {ex.question}", "", f"   <details><summary>Hint</summary>{ex.hint}</details>", ""]
    if outline.further_reading:
        parts += ["## Further reading", "", *[f"- {r}" for r in outline.further_reading], ""]

    post = out / "post.md"
    post.write_text("\n".join(parts).rstrip() + "\n", encoding="utf-8")
    return post


def _anchor(i: int, heading: str) -> str:
    # GitHub/Hugo-style slug of "## {i}. {heading}"
    return re.sub(r"[^a-z0-9 -]", "", f"{i}. {heading}".lower()).strip().replace(" ", "-")
