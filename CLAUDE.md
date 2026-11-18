# CLAUDE.md

Notes for Claude working on ecs-engine. The first section exists to keep token use down:
everything printed into the conversation is re-read on every later turn.

## Keeping token use down

- **Quiet output.** Print only what the next step needs.
  - Builds: show only errors and warnings, e.g.
    `cmake --build --preset debug 2>&1 | grep -E "\): (error|warning)" | head -30`.
    Don't print the full MSBuild log.
  - Tests: show only the summary line, plus failures if there are any:
    `build/msvc/tests/Debug/engine_tests.exe 2>&1 | tail -3`.
  - Git: prefer `git diff --stat` and `git log --oneline -5`; show a full diff only for the
    files being reviewed.
- **Read narrowly.** Search with Grep first, then read the lines you need, by range. Don't
  `cat` several whole headers at once, and don't re-read a file that's already in the
  conversation.
- **One session per feature.** Once a PR is merged, the next task starts in a new session
  rather than continuing a long one.
- **Delegate big, well-defined work** to a Sonnet session with a clear brief. Keep Opus for
  planning and review at checkpoints (design before coding, diff and test results before the PR).
- **Screenshots only to confirm** a visible change in the running game, one per change.
- **Don't poll.** Run long waits (CI, builds) in the background and wait to be notified.
- **Short replies.** Give the result and what's left; skip recaps of earlier turns.

## Project facts (so they needn't be rediscovered)

- Build: `cmake --preset msvc`, then `cmake --build --preset debug` (or `release`).
  Tests: `build/msvc/tests/<Config>/engine_tests.exe` and `game_tests.exe`.
  The game: `build/msvc/game/<Config>/game.exe`.
- CI builds MSVC Debug and Release, GCC and Clang, and treats warnings as errors
  (`/Wall /WX /analyze`, `-Wshadow -Wconversion` and more).
- Format with clang-format:
  `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe -i <files>`.
- Python isn't installed; use bash, sed or PowerShell. Files use LF line endings.
- PRs target `main` and merge with merge commits (`gh pr merge --merge --delete-branch`),
  only after CI passes and the user says so.
- When the game is run for a check, confirm the window has focus before sending keys, and
  back up and restore the real save file (`%APPDATA%\ecs-engine\Tangle Run\save.txt`).
