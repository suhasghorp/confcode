---
name: confcode
description: Turn a YouTube video (conference talk, coding tutorial) into a blog post or textbook-style chapter with all on-screen code extracted, using this repo's confcode CLI. Use when asked to make a post, article, chapter or notes from a YouTube URL, or to re-render/update an existing one (e.g. under cpp/).
---

# ConfCode: conference talk on YouTube → tested Markdown article

The CLI (`.venv/bin/confcode`, source in `src/confcode/`) runs six cached stages: fetch → frames → vision → merge → write → render. Each stage caches to `<out>/.work/`, so a rerun with the same `--out` skips finished stages. `--help` lists every flag.

Two stages need a model: **vision** (transcribe frames) and **write** (outline + one draft per section). `--backend` picks who does them:

- **`session` (default choice for this user).** No API calls. The CLI writes each request to `<out>/.work/session/*.md` (instructions, input, output schema, result path) and exits with code 3. You do the work in this Claude Code session, which runs on the user's Claude subscription, write the result JSON, and rerun the same command. Textbook style only.
- **`api`.** Calls the Anthropic API with `ANTHROPIC_API_KEY`. That is billed separately from a Claude Pro subscription (about $4.50 for a 60-minute textbook chapter). Use it only if the user asks for it.

## Steps

1. **Name the output folder.** Convention: `<language>/<event>/<year>/<topic-slug>` at the repo root, e.g. `cpp/accu/2026/fearless-concurrency`. Take it from the video metadata (`yt-dlp --dump-json <url>`: `title`, `channel`, `upload_date`), or from the user. Done when the folder is fixed *before* the first run: the cache lives inside it, and `.work/*.json` stores repo-relative paths, so a later move means `sed -i 's#old/#new/#g' .work/*.json`.

2. **Download and select frames.** Run `confcode <url> --out <folder> --dry-run`. Done when it reports the candidate frame count (a 60-minute talk gives about 90).

3. **Vision** (session backend). Run the command with `--backend session`. It writes one `batch_*.md` task per 6 frames.
   - **Look for the slide deck first.** If the video description links slides (Speaker Deck, GitHub, a PDF), download the PDF into its own scratchpad folder and run `pdftotext -layout` page by page. Speaker Deck pages carry the PDF URL as `https://files.speakerdeck.com/presentations/…/*.pdf`. Text decks give exact code and are the best source to check transcriptions against. Pop-up boxes that overlap the code scramble its text layer, so read those slides from the frames. If the code in the deck is embedded as images (its text layer has the titles but no code), render the pages with `pdftoppm -r 110 -png` and read the code from the page images; they are sharper than video frames. Make a labelled thumbnail sheet first to map deck pages to slides. Conference decks are often in the event's GitHub repo, e.g. `CppCon/CppCon20XX/Presentations/`; search the tree with the GitHub API.
   - To save usage, tile each batch into a 2×3 contact sheet (PIL, 640×360 per frame, with the frame index and timestamp drawn on). Read the sheets to get slide titles and text.
   - Open only the frames that show code at full resolution, from `.work/frames/`. Crop to the code area and upscale it 1.4× for slides, or 2× for live-demo editor panes. Small terminal text is readable that way.
   - Keep notes (frame index, title, text) and transcriptions in scratchpad files as you go, then generate all batch files with one script.
   - Write each result to `.work/vision/batch_<indices>.json` as `{"frames": [FrameReading, ...]}`. The schema is in the task file.
   - Transcribe code verbatim. Work out indentation from pixel x-offsets (divide by the character width of a long line).
   - Put tables of type names in `slide_text`, not `code_blocks`, so they don't become tiny snippets.
   - Done when every batch file exists. Rerun; the merge prints the snippet count. Check `.work/snippets.json` for sensible merges.

4. **Outline.** The rerun writes `session/outline.md`, which holds the full transcript, the slide timeline and the snippets. Read it, then write `.work/outline.json` (a `ChapterOutline`). Validate it with `ChapterOutline.model_validate` before saving.
   - Section count comes from the task.
   - Ranges are chronological and cover the whole talk.
   - Every snippet ID is assigned to exactly one section.
   - Background sections teach what the talk assumes.
   - `further_reading` lists only things named in the talk.

   Write the outline as a script in `.work/session_src/outline.py`, not inline, so it can be regenerated.

   Rerun to get one `section_XX.md` task per section.

5. **Sections.** Write each section as Markdown in `.work/session_src/section_XX.md`, then convert it with `.venv/bin/python -m confcode.session section <src.md> .work/sections/section_XX.json`. The module docstring in `src/confcode/session.py` describes the format.
   - Place code only through `{{snippet:ID}}` and `{{example:ID}}` marker lines.
   - Put each example in an `@@@ example E1 cpp | Title` … `@@@ end` block.
   - Attribute opinions to the speaker and walk through every talk listing.
   - In example comments, refer to talk listings by snippet ID, written out one by one ("from S03, S04, S05 and S06"). The renderer turns each ID into "Listing N". Ranges like "S04 to S06" render misleadingly.
   - Done when every section JSON exists. Rerun; `[6/6] render` prints the word count.

6. **Verify talk code.**
   - Compare each `<out>/code/S0N_*` with `<out>/frames/S0N.jpg`.
   - If the slides link a code repository, clone it into the scratchpad and diff tokens against the snippets.
   - Compile with both `g++` and `clang++` (Clang 20 is installed). In 2026 one compiler can reject what the speaker's compiler accepted.
   - Assemble the talk's code into a header and compile the talk's demos: the ones that should build, should build and run as shown; the ones that should be rejected, should fail to compile. Differences you find are content: explain them in the chapter.
   - Done when every snippet is checked.

7. **Compile every illustrative example.** Extract them from the section JSONs into the scratchpad. Compile C++ with `g++ -std=c++26 -fsyntax-only`, adding `-freflection` for C++26 reflection (GCC 16 supports it) and `-include <talk header>` for examples that build on talk code. Check Rust with `rustc`. Run a few and confirm that any output the prose quotes is real. Talks about senders or asio need `NVIDIA/stdexec` and `chriskohlhoff/asio` headers in the scratchpad; GCC has no `std::execution`, so use a stdexec shim. Done when every example compiles, or fails only where the prose says it must, or is identified as a fragment.

8. **Check for leaked IDs.** Run `awk '/^```/{c=!c;next} !c' <out>/post.md | grep -nE '\b(S[0-9]{2}|E[0-9]{1,2})\b|\{\{'`. It should print nothing. A hit means an internal snippet or example ID reached the prose: fix it in `render._resolve_refs` / `_ID_REF` and re-render. Re-rendering is free.

9. **Report.** Give the path, the section and word count, the number of talk listings and illustrative listings, the compile results, the cost (none with the session backend), and anything the verification found.

## Known quirks

- **Fixing a transcription after writing.** Patch the code in `.work/vision/batch_*.json` and `.work/readings.json`, then rerun with `--redo merge`. Never use `--redo vision`: it deletes the batch files. With the session backend, an upstream redo keeps the outline and sections; only `--redo write` discards them.
- **Keep slide typos.** If the deck shows a typo, such as a missing parenthesis, transcribe it as shown and mention it in the prose. Never silently correct it.
- **Blocked actions.** Building or running downloaded source is blocked: `./configure` for liburing, and running binaries linked against a cloned library (e.g. ctrack). Compile against cloned headers with `-fsyntax-only` and say in the report that those examples were not run; ask the user; when they allow it, run them (link with -ltbb or define CTRACK_DISABLE_EXECUTION_POLICY for ctrack) and add the measured results to the chapter. liburing-dev is installed: compile io_uring code with `-luring`, and test it by sending datagrams with `echo … > /dev/udp/127.0.0.1/<port>`.

- **Two-column slides.** A slide with two code columns becomes two consecutive listings.
- **Timestamps.** They can be a few seconds off: section downloads skip `force_keyframes_at_cuts` because it re-encodes and is slow.
- **No fuzzy merging.** Merging is deterministic, so similar code such as `thread` and `jthread` stays separate. Run `.venv/bin/pytest` after changing `merge.py`, `frames.py`, `render.py` or `session.py`.
- **Frame selection.** It masks motion, such as the speaker's camera. If the frame count is very high or very low, adjust `--interval` / `--min-samples`, or `busy_share` / the thresholds in `frames.py`.
- **GCC 16 reflection.** It reports no data members for lambda closure types, so reflection-based checks pass any lambda. Use named function objects when an example must show a rejection.
- **`pkill -f confcode`.** The pattern matches your own shell and kills it. Stop a background run by its PID or task ID.
