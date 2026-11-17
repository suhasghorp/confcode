"""Data models shared across pipeline stages, plus the schemas Claude must fill."""

from __future__ import annotations

from pydantic import BaseModel, Field


# ---------- pipeline data ----------


class TranscriptSegment(BaseModel):
    start: float  # seconds, absolute in the source video
    end: float
    text: str


class VideoInfo(BaseModel):
    url: str
    video_id: str
    title: str
    channel: str = ""
    upload_date: str = ""  # YYYYMMDD
    description: str = ""
    duration: float = 0
    chapters: list[dict] = Field(default_factory=list)  # {start_time, end_time, title}
    start: float = 0  # processed range (absolute seconds)
    end: float = 0
    video_path: str = ""


class Frame(BaseModel):
    index: int
    ts: float  # absolute seconds in the source video
    path: str


class Snippet(BaseModel):
    id: str
    language: str
    filename_hint: str = ""
    code: str
    first_ts: float
    last_ts: float
    frame_path: str  # frame showing the most complete version
    slide_title: str = ""


# ---------- Claude structured-output schemas ----------


class CodeOnScreen(BaseModel):
    language: str = Field(
        description="Lowercase fence language, e.g. cpp, python, rust, console, text"
    )
    filename_hint: str = Field(
        description="File name visible in a tab/title bar, or empty string"
    )
    code: str = Field(description="Verbatim code with exact indentation")
    is_partial: bool = Field(
        description="True if the code is visibly cut off (scrolled, cropped, mid-typing)"
    )


class FrameReading(BaseModel):
    frame_index: int
    slide_title: str = Field(description="Slide/screen heading, or empty string")
    slide_text: str = Field(
        description="Non-code prose/bullets on screen, condensed; empty if none"
    )
    code_blocks: list[CodeOnScreen]


class BatchReading(BaseModel):
    frames: list[FrameReading]


class PostSection(BaseModel):
    heading: str
    start_seconds: int
    body_md: str = Field(
        description="Markdown prose for this section. Never include code blocks."
    )
    snippet_ids: list[str] = Field(
        description="IDs of snippets to show in this section, in display order"
    )


class PostPlan(BaseModel):
    title: str
    summary: str
    tags: list[str]
    sections: list[PostSection]


# ---------- textbook-style chapter ----------


class OutlineSection(BaseModel):
    heading: str
    start_seconds: int
    end_seconds: int
    snippet_ids: list[str] = Field(description="Talk snippets this section explains")
    key_points: list[str] = Field(description="What the talk says here, as concise bullet facts")
    teaching_plan: str = Field(
        description="Background a student needs here and what extra illustrative examples would help"
    )


class GlossaryEntry(BaseModel):
    term: str
    definition: str


class Exercise(BaseModel):
    question: str
    hint: str


class ChapterOutline(BaseModel):
    title: str
    introduction: str = Field(description="1-3 paragraphs of Markdown motivating the chapter")
    learning_objectives: list[str]
    prerequisites: list[str]
    tags: list[str]
    sections: list[OutlineSection]
    key_takeaways: list[str]
    glossary: list[GlossaryEntry]
    exercises: list[Exercise]
    further_reading: list[str] = Field(
        description="Only libraries/papers/talks actually named in the talk or on slides"
    )


class Example(BaseModel):
    id: str = Field(description="E1, E2, ... unique within the section")
    title: str
    language: str
    code: str


class SectionDraft(BaseModel):
    body_md: str = Field(
        description="Markdown body. Place code ONLY via {{snippet:ID}} and {{example:ID}} markers on their own lines."
    )
    examples: list[Example]
