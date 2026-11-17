"""Helpers for --backend session: turn hand-written Markdown into the JSON a task expects.

Section source format (one file per section):

    Markdown body with {{snippet:S03}} / {{example:E1}} marker lines ...

    @@@ example E1 cpp | Title of the example
    code ...
    @@@ end

Example blocks may appear anywhere; they are removed from the body.

Usage:  python -m confcode.session section SRC.md OUT.json
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from .models import Example, SectionDraft

_START = re.compile(r"^@@@ example (\w+) (\S+) \| (.+)$")


def parse_section(text: str) -> SectionDraft:
    body: list[str] = []
    examples: list[Example] = []
    cur: dict | None = None
    for line in text.splitlines():
        m = _START.match(line)
        if cur is None and m:
            cur = {"id": m.group(1), "language": m.group(2), "title": m.group(3).strip(), "code": []}
        elif cur is not None and line.strip() == "@@@ end":
            examples.append(Example(id=cur["id"], title=cur["title"], language=cur["language"],
                                    code="\n".join(cur["code"]).strip("\n")))
            cur = None
        elif cur is not None:
            cur["code"].append(line)
        else:
            body.append(line)
    if cur is not None:
        raise ValueError(f"example {cur['id']} is missing '@@@ end'")
    body_md = re.sub(r"\n{3,}", "\n\n", "\n".join(body)).strip()
    ids = {e.id for e in examples}
    placed = set(re.findall(r"\{\{example:(\w+)\}\}", body_md))
    if placed - ids:
        raise ValueError(f"markers without examples: {sorted(placed - ids)}")
    return SectionDraft(body_md=body_md, examples=examples)


def main(argv: list[str]) -> None:
    if len(argv) != 3 or argv[0] != "section":
        sys.exit(__doc__)
    draft = parse_section(Path(argv[1]).read_text(encoding="utf-8"))
    Path(argv[2]).write_text(draft.model_dump_json(indent=1), encoding="utf-8")
    print(f"{argv[2]}: {len(draft.body_md.split())} words, {len(draft.examples)} example(s)")


if __name__ == "__main__":
    main(sys.argv[1:])
