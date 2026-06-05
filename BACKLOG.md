# MediaCurator — Backlog

Items are tagged: `[P1]` critical path, `[P2]` high value, `[P3]` nice-to-have
Status: `[ ]` todo · `[~]` in progress · `[x]` done · `[-]` deferred

---

## 🚀 SHIP QUEUE (current sprint)
> These items are ready to be worked. Pick from the top.

- [ ] `[P1]` **CMakeLists.txt** — root + src/CMakeLists with Qt6, nlohmann/json via vcpkg, MSVC config
- [ ] `[P1]` **DatabaseManager** — schema init, CRUD for files/streams/jobs tables
- [ ] `[P1]` **FfprobeScanner** — invoke ffprobe -v quiet -print_format json -show_streams -show_format, parse result, insert into DB
- [ ] `[P1]` **McMainWindow skeleton** — menu bar, splitter, status bar, central QTableView placeholder
- [ ] `[P1]` **McTrackTableModel** — QAbstractTableModel reading streams+files JOIN from DB
- [ ] `[P1]` **ScanWorker** — QThread, scans a folder recursively for video files, emits progress
- [ ] `[P1]` **ExternalTools** — locate ffprobe.exe/mkvmerge.exe in tools/ relative to exe, validate version

---

## Phase 1 — Foundation

- [ ] `[P1]` Basic column sort/filter on the main table (QSortFilterProxyModel)
- [ ] `[P1]` Persistent window geometry (QSettings)
- [ ] `[P2]` File context menu: open folder, open in VLC
- [ ] `[P2]` Scan progress dialog with cancel support
- [ ] `[P2]` Incremental rescan (only changed files based on mtime/size)
- [ ] `[P2]` Multi-root library support (scan multiple folders)
- [ ] `[P3]` File watcher (QFileSystemWatcher) for live library updates

---

## Phase 2 — Rule Engine & Preview

- [ ] `[P1]` **UserProfile** model + JSON serialization to DB prefs table
- [ ] `[P1]` **McSettingsDialog** — understood languages, keep-original toggle, preferred codec order
- [ ] `[P1]` **OriginalLanguageDetector** — check file tags, fall back to majority audio language
- [ ] `[P1]` **CodecHierarchy** — built-in table: TrueHD⊃AC3, DTS-HD MA⊃DTS, etc.
- [ ] `[P1]` **RuleEngine::evaluateFile()** — returns TrackDecision (KEEP/REMOVE/UNSURE) per stream
- [ ] `[P1]` **McPreviewDialog** — before/after track table for one file, shows reason per track
- [ ] `[P2]` Estimated size saving calculation (bitrate × duration for removed tracks)
- [ ] `[P2]` Filter panel: "show only files with redundant audio"
- [ ] `[P2]` Filter panel: "show files with tracks in languages not in my profile"
- [ ] `[P2]` Bulk preview — apply rules across all selected files, show aggregate summary
- [ ] `[P3]` Rule conflict detection (e.g. only audio track would be removed)
- [ ] `[P3]` Per-file override — manually pin a track to KEEP or REMOVE regardless of policy

---

## Phase 3 — Action Engine

- [ ] `[P1]` **ActionEngine::buildCommand()** — mkvmerge args from TrackDecisions
- [ ] `[P1]` **RemuxJob** — QProcess wrapper, captures stdout/stderr, emits progress
- [ ] `[P1]` **JobQueue** — sequential job runner, pause/resume/cancel
- [ ] `[P1]` **McJobPanel** — bottom panel: job list, progress bar, dry-run toggle, run button
- [ ] `[P1]` Dry-run mode — shows mkvmerge command without executing
- [ ] `[P1]` Post-job rescan — update DB after successful remux
- [ ] `[P1]` Atomic rename — temp output → original path after verification
- [ ] `[P2]` Job history log (persist to DB)
- [ ] `[P2]` Space-saved tracker (cumulative bytes freed)
- [ ] `[P2]` Undo via trash (move original to OS recycle bin before replace)
- [ ] `[P3]` Parallel jobs (configurable worker count)
- [ ] `[P3]` Job export — save mkvmerge script as .bat/.sh for manual review

---

## Phase 4 — Track Classifier

- [ ] `[P1]` `data/track_classifier.json` — regex/keyword rules for commentary, SDH, forced, signs
- [ ] `[P1]` **RegexClassifier** — fast keyword matching, O(1) per track title
- [ ] `[P1]` Commentary tracks: apply keep policy based on understood languages
- [ ] `[P2]` SDH/HI subtitles: configurable keep/remove separately from regular subtitles
- [ ] `[P2]` Forced subtitle detection (flag + title heuristic)
- [ ] `[P2]` Classifier confidence shown as tooltip in track table
- [ ] `[P3]` **OnnxClassifier** — optional download, ~50MB, better accuracy on unusual titles
- [ ] `[P3]` **ApiClassifier** — Claude API, user provides key, fractions of a cent/file
- [ ] `[P3]` User-trainable classifier — "this track is commentary, remember it" feedback loop

---

## Phase 5 — Non-MKV & Packaging

- [ ] `[P1]` Non-MKV remux flow — mkvmerge from mp4/avi input with track filter in single pass
- [ ] `[P1]` scripts/setup_tools.ps1 — auto-download ffprobe + mkvmerge portable (Windows)
- [ ] `[P1]` scripts/setup_tools.sh — same for Linux/macOS
- [ ] `[P2]` CPack NSIS installer (Windows) — bundles tools/, Qt DLLs, MSVC redistributable
- [ ] `[P2]` CPack DMG (macOS)
- [ ] `[P2]` CPack AppImage (Linux)
- [ ] `[P2]` About dialog with version, license info, donation link (Ko-fi / GitHub Sponsors)
- [ ] `[P2]` GitHub Actions CI — Windows build + test on push
- [ ] `[P3]` macOS/Linux CI jobs
- [ ] `[P3]` Automatic update check (compare GitHub release tag)

---

## Future / Icebox

- [ ] `[P3]` HDR metadata display (MaxCLL, MaxFALL, mastering display)
- [ ] `[P3]` Duplicate file detection (same title, different quality)
- [ ] `[P3]` Chapter editor (view/remove chapters)
- [ ] `[P3]` Attachment management (embedded fonts, cover art)
- [ ] `[P3]` Export library to CSV/JSON for external scripting
- [ ] `[P3]` Dark mode / theme support
- [ ] `[P3]` Keyboard shortcuts for power users
- [ ] `[P3]` "What if" simulation — shows library size after applying policy to everything

---

## Bug Tracker

> Add bugs here as found during development.

- (none yet)

---

## Done ✅

> Move items here when merged to main.

(nothing done yet — project just started)
