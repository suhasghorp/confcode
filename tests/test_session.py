import pytest

from confcode.session import parse_section


def test_parse_section_splits_body_and_examples():
    src = """Intro text.

{{example:E1}}

More text.

@@@ example E1 cpp | A title
int main() {}
@@@ end
"""
    d = parse_section(src)
    assert d.body_md == "Intro text.\n\n{{example:E1}}\n\nMore text."
    assert [(e.id, e.language, e.title, e.code) for e in d.examples] == [("E1", "cpp", "A title", "int main() {}")]


def test_parse_section_rejects_missing_example():
    with pytest.raises(ValueError):
        parse_section("{{example:E2}}")
