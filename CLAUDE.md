# MediaCurator — Claude Code Context

## Project Summary
MediaCurator is an open-source (Apache 2.0) C++20/Qt6 desktop application for curating
personal video libraries. It scans media files using ffprobe, stores metadata in SQLite,
applies smart policy rules to identify redundant/unwanted tracks, and uses mkvmerge to
losslessly remove them. Think MovieScanner2 + smart language/codec rules + bulk editing.

## Owner
Jacob @ Lasernet Group (personal project, not work-related)

## Tech Stack
- **Language**: C++20
- **UI**: Qt6 (Widgets)
- **Build**: CMake 3.25+ with CPack for installers
- **Database**: SQLite via Qt's QSqlDatabase
- **JSON parsing**: nlohmann/json (header-only)
- **Metadata**: ffprobe (bundled, LGPL build, invoked as subprocess)
- **File editing**: mkvmerge + mkvpropedit (bundled, GPL, invoked as subprocess)
- **Track classification**: Regex/keyword baseline; ONNX Runtime optional (future)
- **IDE**: Visual Studio 2026 (MSVC toolchain, x64)
- **CI**: GitHub Actions (Windows primary, macOS/Linux secondary)

## Build Instructions
```bash
# Windows (from repo root, using VS Developer PowerShell)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Or open build/MediaCurator.sln directly in Visual Studio 2026
```

## Project Structure
```
MediaCurator/
├── CLAUDE.md              ← You are here
├── PLANNING.md            ← Architecture decisions and design
├── BACKLOG.md             ← Feature backlog with ship queue
├── CMakeLists.txt         ← Root build file (owns all)
├── cmake/                 ← CMake helper modules
├── src/
│   ├── main.cpp
│   ├── core/              ← DatabaseManager, AppSettings, UserProfile
│   ├── scanner/           ← FfprobeScanner, FileWatcher
│   ├── engine/            ← RuleEngine, ActionEngine, JobQueue
│   ├── classifier/        ← TrackClassifier (regex + ONNX)
│   └── ui/                ← MainWindow, TrackTableModel, FilterPanel, JobPanel
├── include/               ← Matching header tree
├── resources/             ← Qt .qrc resources (icons, etc.)
├── docs/                  ← Screenshots, wiki content
├── scripts/               ← Helper scripts (bundle ffprobe, package, etc.)
└── .claude/
    └── agents/            ← Agent sub-instructions
```

## Agent Roles (multi-agent workflow)
See `.claude/agents/` for role-specific instructions:
- **developer.md** — implements features from BACKLOG.md
- **qa.md** — writes and runs tests, validates builds
- **reviewer.md** — reviews code for correctness, style, architecture

## Coding Conventions
- Headers in `include/`, implementations in `src/`, mirrored subdirectory structure
- All Qt classes prefixed with `Mc` (e.g., `McMainWindow`, `McTrackTableModel`)
- Use `Q_OBJECT` macro in all QObject subclasses
- Prefer `std::filesystem` for path operations, `QProcess` for subprocesses
- Database access only through `DatabaseManager` singleton — never raw SQL in UI code
- All ffprobe/mkvmerge invocations go through `ExternalTools` wrapper class
- Prefer `enum class` over plain `enum`
- Use `[[nodiscard]]` on functions returning error codes / results
- clang-format style: `{ BasedOnStyle: Microsoft }` (see `.clang-format`)

## Key Design Principles
1. **Non-destructive by default** — no file is touched until user explicitly confirms
2. **Scan → Store → Query → Act** pipeline — UI always reads from DB, never live files
3. **Subprocess isolation** — ffprobe and mkvmerge are external tools, never linked
4. **Policy-driven** — all keep/remove decisions derive from UserProfile preferences
5. **Preview before action** — every ActionEngine operation has a dry-run mode

## Database Schema (core tables)
See `src/core/DatabaseManager.cpp` for the full schema. Key tables:
- `files` — one row per scanned file (path, size, container, duration)
- `streams` — one row per track in a file (codec, language, type, flags)
- `scan_runs` — history of scan operations
- `jobs` — pending/completed mkvmerge jobs

## Environment Requirements
- Qt 6.7+ (set `Qt6_DIR` in CMake or via Qt Maintenance Tool)
- MSVC 2022 v17+ (shipped with Visual Studio 2026)
- CMake 3.25+
- vcpkg (bootstrapped as git submodule at `vcpkg/`)
- ffprobe.exe and mkvmerge.exe placed in `tools/` after first build (see scripts/setup_tools.ps1)
