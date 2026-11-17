from confcode.models import PostPlan, PostSection, Snippet, VideoInfo
from confcode.render import fence, render, snippet_filename


def _snip(i, code, lang="cpp", slide=""):
    return Snippet(id=f"S{i:02d}", language=lang, code=code, first_ts=100 + i, last_ts=100 + i,
                   frame_path="", slide_title=slide)


def test_fence_escapes_backticks():
    assert fence("a ``` b", "md").startswith("````md\n")


def test_render_includes_all_snippets(tmp_path):
    info = VideoInfo(url="https://www.youtube.com/watch?v=abc", video_id="abc", title='A "talk"',
                     channel="Chan")
    snips = [_snip(1, "int x;", slide="Senders: intro"), _snip(2, "ls -la", "bash")]
    plan = PostPlan(title="Post", summary="Sum.", tags=["cpp"], sections=[
        PostSection(heading="Intro", start_seconds=101, body_md="Text.", snippet_ids=["S01", "S99"])
    ])
    post = render(info, plan, snips, tmp_path, save_frames=False).read_text()
    assert post.startswith("---\ntitle: \"Post\"")
    assert 'video_title: "A \\"talk\\""' in post
    assert "```cpp\nint x;\n```" in post
    assert "## Additional code shown" in post and "```bash\nls -la\n```" in post
    assert "watch?v=abc&t=101s" in post
    assert snippet_filename(snips[0]) == "S01_senders_intro.cpp"
    assert (tmp_path / "code" / "S02_bash.sh").read_text() == "ls -la\n"


def test_textbook_markers_and_labels(tmp_path):
    from confcode.models import ChapterOutline, Example, OutlineSection, SectionDraft
    from confcode.render import render_textbook

    info = VideoInfo(url="https://www.youtube.com/watch?v=abc", video_id="abc", title="T", channel="C")
    snips = [_snip(1, "int x;\n\nint y;"), _snip(2, "int z;")]
    outline = ChapterOutline(
        title="Ch", introduction="Intro.", learning_objectives=["o"], prerequisites=["p"], tags=["t"],
        sections=[OutlineSection(heading="One", start_seconds=0, end_seconds=60,
                                 snippet_ids=["S01", "S02"], key_points=[], teaching_plan="")],
        key_takeaways=["k"], glossary=[], exercises=[], further_reading=[])
    drafts = [SectionDraft(body_md="Before.\n{{snippet:S01}}\nMid, unlike Example E1 and S02.\n{{example:E1}}\n{{example:E9}}\nEnd.",
                           examples=[Example(id="E1", title="Callback version", language="cpp",
                                             code="f(g); // see S01")])]
    post = render_textbook(info, outline, drafts, snips, tmp_path, save_frames=False).read_text()
    assert "**Listing 1** — code from the talk" in post and "int x;\n\nint y;" in post
    assert "**Listing 2** — Callback version *(illustrative example, not from the talk)*" in post
    assert "{{" not in post
    assert "**Listing 3** — code from the talk" in post and "int z;" in post  # unplaced S02 appended
    assert (tmp_path / "code" / "examples" / "01_E1_callback_version.cpp").exists()
    assert "unlike Listing 2 and Listing 3." in post and "f(g); // see Listing 1" in post
