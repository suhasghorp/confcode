"""Thin wrapper around the Anthropic SDK for structured-output calls."""

from __future__ import annotations

import json
from pathlib import Path
from typing import TypeVar

import anthropic
from pydantic import BaseModel

T = TypeVar("T", bound=BaseModel)

DEFAULT_MODEL = "claude-opus-5-5"
FALLBACK_BETA = "server-side-fallback-2026-07-01"

_client: anthropic.Anthropic | None = None


class LLMError(RuntimeError):
    pass


class PendingTasks(Exception):
    """Session backend: requests were written to disk instead of sent; results are pending."""

    def __init__(self, paths: list[Path]):
        super().__init__(f"{len(paths)} pending task(s)")
        self.paths = paths


# When set, parse() writes each request to this directory instead of calling the API,
# so a Claude Code session can produce the result file itself (see .claude/skills/confcode).
SESSION_DIR: Path | None = None


def _write_task(name: str, result_path: Path, schema: type[BaseModel], content, system: str) -> Path:
    SESSION_DIR.mkdir(parents=True, exist_ok=True)
    blocks = [content] if isinstance(content, str) else [b["text"] for b in content if b.get("type") == "text"]
    task = SESSION_DIR / f"{name}.md"
    task.write_text(
        f"# Task: {name}\n\n"
        f"Write the result as JSON matching the schema below to:\n\n    {result_path.resolve()}\n\n"
        f"## Instructions\n\n{system}\n\n## Input\n\n" + "\n\n".join(blocks)
        + f"\n\n## Output schema ({schema.__name__})\n\n```json\n"
        + json.dumps(schema.model_json_schema(), indent=1) + "\n```\n",
        encoding="utf-8",
    )
    return task


def client() -> anthropic.Anthropic:
    global _client
    if _client is None:
        # Resolves ANTHROPIC_API_KEY / ANTHROPIC_AUTH_TOKEN / `ant auth login` profile.
        _client = anthropic.Anthropic(max_retries=4)
    return _client


def parse(
    *,
    schema: type[T],
    content: list[dict] | str,
    system: str,
    model: str = DEFAULT_MODEL,
    effort: str = "low",
    max_tokens: int = 16000,
    task: tuple[str, Path] | None = None,
) -> tuple[T, dict]:
    """One structured-output request. Returns (parsed object, usage dict).

    ``task`` = (name, result_path) is used by the session backend.
    """
    if SESSION_DIR is not None:
        if task is None:
            raise LLMError("This stage does not support --backend session")
        raise PendingTasks([_write_task(task[0], task[1], schema, content, system)])
    # Streaming: large max_tokens values are rejected for non-streaming calls.
    with client().beta.messages.stream(
        model=model,
        max_tokens=max_tokens,
        system=system,
        messages=[{"role": "user", "content": content}],
        output_config={"effort": effort},
        output_format=schema,
        betas=[FALLBACK_BETA],
        fallbacks="default",  # on a policy decline, rerun on a fallback model
    ) as stream:
        resp = stream.get_final_message()
    if resp.stop_reason == "refusal":
        cat = getattr(resp.stop_details, "category", None) if resp.stop_details else None
        raise LLMError(f"Request refused (category={cat})")
    if resp.stop_reason == "max_tokens":
        raise LLMError(f"Output hit max_tokens={max_tokens}; raise it or shrink batches")
    if resp.parsed_output is None:
        raise LLMError(f"No parsed output (stop_reason={resp.stop_reason})")
    u = resp.usage
    usage = {"input_tokens": u.input_tokens, "output_tokens": u.output_tokens}
    return resp.parsed_output, usage
