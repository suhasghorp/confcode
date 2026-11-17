from confcode.merge import contains, merge, normalize, stitch
from confcode.models import CodeOnScreen, Frame, FrameReading


def _run(codes, lang="cpp", hints=None):
    frames = [Frame(index=i, ts=i * 10.0, path=f"f{i}.jpg") for i in range(len(codes))]
    readings = {
        i: FrameReading(frame_index=i, slide_title="", slide_text="", code_blocks=[
            CodeOnScreen(language=lang, filename_hint=(hints or [""] * len(codes))[i], code=c, is_partial=False)
        ] if c else [])
        for i, c in enumerate(codes)
    }
    return merge(readings, frames)


A = "int main() {\n    auto s = schedule(pool);\n    return 0;\n}"


def test_identical_views_collapse():
    out = _run([A, A, A])
    assert len(out) == 1 and out[0].first_ts == 0 and out[0].last_ts == 20


def test_typing_progression_keeps_final():
    steps = ["int main() {", "int main() {\n    auto s = sched", A]
    out = _run(steps)
    assert len(out) == 1 and out[0].code == A and out[0].frame_path == "f2.jpg"


def test_progressive_slide_reveal():
    a = "auto s = just(42);"
    b = a + "\nauto t = then(s, f);"
    c = b + "\nsync_wait(t);"
    assert [s.code for s in _run([a, b, c])] == [c]


def test_partial_highlight_dropped():
    assert len(_run([A, "    auto s = schedule(pool);"])) == 1


def test_before_after_refactor_kept_separate():
    before = "void f() {\n    std::thread t(work);\n    t.join();\n}"
    after = "void f() {\n    std::jthread t(work);\n    t.join();\n}"
    out = _run([before, after])
    assert [s.code for s in out] == [before, after]


def test_one_char_difference_is_a_distinct_version():
    changed = A.replace("return 0", "return 1")
    assert len(_run([A, changed])) == 2


def test_unfinished_line_must_be_prefix():
    # last line differs but is not a typing prefix -> separate snippets
    assert len(_run(["a();\nb();", "a();\nc();"])) == 2


def test_scroll_stitch():
    top = "\n".join(f"line{i};" for i in range(0, 10))
    bottom = "\n".join(f"line{i};" for i in range(6, 16))
    out = _run([top, bottom])
    assert len(out) == 1
    assert out[0].code == "\n".join(f"line{i};" for i in range(16))


def test_different_files_not_merged():
    assert len(_run([A, A], hints=["a.cpp", "b.cpp"])) == 2


def test_no_code_frames_ignored_and_ids_ordered():
    out = _run(["", A, "", "x = 1;\ny = 2;"])
    assert [s.id for s in out] == ["S01", "S02"]


def test_normalize_dedents_and_trims():
    assert normalize("\n    a\n      b\n\n") == "a\n  b"


def test_contains_and_stitch_helpers():
    assert contains("a\nb\nc", "b\nc")
    assert not contains("a\nb", "a\nz")
    assert contains("a\nb\nc\nd_full", "a\nb\nc\nd_fu", allow_unfinished_last=True)
    assert stitch("a\nb", "c\nd") is None
