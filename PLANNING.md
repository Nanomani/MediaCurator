# MediaCurator — Architecture & Planning

## Vision
A desktop GUI application that lets you:
1. **Scan** your video library and store all track metadata in a local SQLite database
2. **Query** with smart rules that understand codec hierarchy, original language, commentary tracks, etc.
3. **Act** on results — remove redundant or unwanted tracks using mkvmerge (losslessly)

Fills the gap between MovieScanner2 (read-only, limited filtering) and Tdarr (server-based,
transcoding-focused, no smart lossless curation).

---

## License
**Apache 2.0** — open source, donationware model (in-app donation link).
All bundled dependencies are compatible:
- ffprobe: LGPL 2.1+ build (dynamic-link compatible with Apache 2.0)
- mkvmerge: GPL 2.0, but invoked as a **subprocess** — not linked. No license contamination.
- Qt6: LGPL 2.1+, dynamically linked
- SQLite: Public domain
- nlohmann/json: MIT

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        Qt6 MainWindow (McMainWindow)              │
│  ┌──────────────┐  ┌─────────────────┐  ┌────────────────────┐  │
│  │ LibraryPanel │  │ TrackTableView  │  │ FilterPanel         │  │
│  │ (folder tree)│  │ (QTableView +   │  │ (rule builder UI)  │  │
│  └──────────────┘  │  McTrackModel)  │  └────────────────────┘  │
│                    └─────────────────┘                            │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │         ActionPanel / JobQueue (McJobPanel)                │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────┬───────────────────────────────────┘
                               │
┌──────────────────────────────▼───────────────────────────────────┐
│                          Core Library                             │
│  FfprobeScanner │ RuleEngine │ ActionEngine │ TrackClassifier    │
└──────┬──────────┴─────┬──────┴──────┬───────┴─────┬─────────────┘
       │                │             │              │
   ffprobe.exe      DatabaseManager  JobQueue    Regex / ONNX
   (subprocess)     (SQLite/Qt)    (mkvmerge    classifier
                                   subprocesses)
```

---

## Database Schema

```sql
-- One row per scanned file
CREATE TABLE files (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    path          TEXT UNIQUE NOT NULL,
    filename      TEXT NOT NULL,
    size_bytes    INTEGER NOT NULL,
    container     TEXT,           -- mkv, mp4, avi, m2ts ...
    duration_s    REAL,
    overall_bitrate INTEGER,
    original_language TEXT,       -- detected or inferred, ISO 639-2
    scan_time     INTEGER NOT NULL,  -- Unix timestamp
    scan_run_id   INTEGER REFERENCES scan_runs(id),
    needs_rescan  INTEGER DEFAULT 0
);

-- One row per stream/track
CREATE TABLE streams (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id       INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
    stream_index  INTEGER NOT NULL,   -- ffprobe stream index
    codec_type    TEXT NOT NULL,      -- video | audio | subtitle | data
    codec_name    TEXT,               -- h264, hevc, aac, ac3, dts, eac3, truehd, flac...
    language      TEXT,               -- ISO 639-2 (eng, dan, deu, jpn...)
    title         TEXT,               -- track title as stored in file
    track_type    TEXT,               -- classified: main|commentary|sdh|forced|signs|hearing_impaired
    type_confidence REAL,             -- 0.0-1.0 from classifier
    channels      INTEGER,            -- audio: channel count
    sample_rate   INTEGER,            -- audio: Hz
    bit_rate      INTEGER,            -- bits/s
    width         INTEGER,            -- video: pixels
    height        INTEGER,            -- video: pixels
    hdr_format    TEXT,               -- SDR|HDR10|HDR10+|DolbyVision|HLG
    is_default    INTEGER DEFAULT 0,
    is_forced     INTEGER DEFAULT 0,
    is_hearing_impaired INTEGER DEFAULT 0,
    is_visual_impaired  INTEGER DEFAULT 0,
    pixel_format  TEXT,               -- video: yuv420p, yuv420p10le...
    frame_rate    TEXT,               -- video: "23.976"
    codec_level   TEXT,               -- video: codec profile level
    codec_profile TEXT,               -- video: High, Main, etc.
    extra_json    TEXT                -- raw ffprobe tags for anything not mapped
);

-- Codec hierarchy for redundancy detection
CREATE TABLE codec_hierarchy (
    lossless_codec TEXT NOT NULL,     -- truehd, dtshd_ma, flac, pcm_*
    lossy_codec    TEXT NOT NULL,     -- ac3, dts, eac3, aac
    notes          TEXT
);

-- Scan history
CREATE TABLE scan_runs (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    start_time    INTEGER NOT NULL,
    end_time      INTEGER,
    root_path     TEXT NOT NULL,
    files_scanned INTEGER DEFAULT 0,
    files_added   INTEGER DEFAULT 0,
    files_updated INTEGER DEFAULT 0,
    files_removed INTEGER DEFAULT 0
);

-- Pending/completed mkvmerge jobs
CREATE TABLE jobs (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id       INTEGER REFERENCES files(id),
    status        TEXT DEFAULT 'pending',  -- pending|running|done|failed|cancelled
    job_type      TEXT NOT NULL,           -- remux|tag_edit
    command_args  TEXT NOT NULL,           -- JSON array of mkvmerge args
    dry_run       INTEGER DEFAULT 1,
    created_at    INTEGER NOT NULL,
    started_at    INTEGER,
    finished_at   INTEGER,
    result_code   INTEGER,
    output_log    TEXT,
    estimated_saving_bytes INTEGER
);
```

---

## Rule Engine Design

Rules operate on **per-file stream groups**, not individual rows. They are evaluated in order:

### Tier 1 — Codec Hierarchy (objective, always enabled)
Built-in knowledge:
```
TrueHD Atmos  ⊃  TrueHD  ⊃  EAC3 (Atmos core)  ⊃  AC3
DTS-HD MA     ⊃  DTS core
FLAC/PCM      ⊃  (no lossy equivalent)
```
Rule: if a file has `[lossless, lang=X]` AND `[lossy, lang=X]`, the lossy is **REDUNDANT**.

### Tier 2 — Language Policy (user preference profile)
```json
{
  "understood_languages": ["dan", "eng"],
  "always_keep_original_audio": true,
  "keep_commentary_if_in_understood_language": true,
  "keep_forced_subtitles_for_all": true,
  "keep_original_language_subtitle_always": false,
  "preferred_audio_codec_order": ["truehd", "dtshd_ma", "eac3", "dts", "ac3", "aac"],
  "audio_keep_rule": "best_per_language",
  "subtitle_keep_languages": ["dan", "eng"],
  "remove_sdh_subtitles": false,
  "remove_commentary_subtitles": false
}
```

Resolution for Der Untergang (original: `deu`):
- `deu` audio TrueHD → KEEP (original language, always_keep_original_audio=true)
- `eng` audio AC3 → REMOVE (dubbed, not original; user understands EN but original rule wins)
- `deu` audio commentary AAC → KEEP (commentary in understood? No. But original lang... configurable)
- `eng` subtitle → KEEP (understood language)
- `dan` subtitle → KEEP (understood language)
- `deu` subtitle SDH → configurable

### Tier 3 — Track Type Classification
Classify each track's `title` field into a semantic type:
```
main          — no special marker
commentary    — "Commentary", "Kommentar", "Director's", "Cast & Crew"
sdh           — "SDH", "CC", "Closed Caption", "Hörgeschädigte", "HI"
forced        — "Forced", "Forced Subtitles"
signs_songs   — "Signs & Songs", "Signs/Songs", "Song translations"
hearing_impaired — redundant with SDH for most purposes
```
Implementation order:
1. Regex/keyword dictionary (shipped in `data/track_classifier.json`)
2. Optional: ONNX micro-classifier (downloadable, ~50MB)
3. Optional: Claude API (for users with API key — fractions of a cent/file)

---

## Key Classes

### Core
| Class | Responsibility |
|-------|---------------|
| `DatabaseManager` | Singleton. Schema init, all SQL operations, migration |
| `AppSettings` | Qt QSettings wrapper for app-level prefs |
| `UserProfile` | User's language policy, serialized to JSON in DB |
| `ExternalTools` | Locates ffprobe/mkvmerge, validates versions |

### Scanner
| Class | Responsibility |
|-------|---------------|
| `FfprobeScanner` | Invokes ffprobe, parses JSON, populates DB |
| `ScanWorker` | QThread worker, emits progress signals |
| `OriginalLanguageDetector` | Heuristics to detect a file's original language |

### Engine
| Class | Responsibility |
|-------|---------------|
| `RuleEngine` | Evaluates policy rules against DB, returns decisions |
| `TrackDecision` | Enum: KEEP / REMOVE / UNSURE with reason string |
| `ActionEngine` | Builds mkvmerge commands from decisions |
| `JobQueue` | Manages pending jobs, runs them sequentially |
| `RemuxJob` | Single mkvmerge invocation with progress tracking |

### Classifier
| Class | Responsibility |
|-------|---------------|
| `TrackClassifier` | Interface: classifyTrack(title, lang, codec) → TrackType |
| `RegexClassifier` | Fast regex/keyword implementation |
| `OnnxClassifier` | Optional ONNX Runtime implementation |
| `ApiClassifier` | Optional Claude API implementation |

### UI
| Class | Responsibility |
|-------|---------------|
| `McMainWindow` | Main window, menu bar, splitter layout |
| `McLibraryPanel` | Left panel: folder tree, scan button |
| `McTrackTableModel` | QAbstractTableModel over DB streams |
| `McFilterPanel` | Right panel: rule builder, language selector |
| `McJobPanel` | Bottom panel: job queue, progress, dry-run toggle |
| `McPreviewDialog` | Shows before/after for a single file's tracks |
| `McSettingsDialog` | User profile editor, external tools config |

---

## Build Phases

### Phase 1 — Scanner + DB + Basic Table ✅ (start here)
- [ ] CMakeLists.txt with vcpkg, Qt6, nlohmann/json
- [ ] DatabaseManager (schema init, CRUD)
- [ ] FfprobeScanner (ffprobe JSON → DB)
- [ ] McMainWindow skeleton with QTableView
- [ ] ScanWorker (threaded scan with progress bar)
- [ ] Basic column sorting/filtering

### Phase 2 — Rule Engine + Preview
- [ ] UserProfile model + McSettingsDialog
- [ ] OriginalLanguageDetector
- [ ] Codec hierarchy table + redundancy detection
- [ ] Language policy evaluation
- [ ] McPreviewDialog (before/after track list)
- [ ] Estimated size savings calculation

### Phase 3 — Action Engine
- [ ] ExternalTools (locate/validate ffprobe + mkvmerge)
- [ ] ActionEngine (build mkvmerge commands)
- [ ] JobQueue + RemuxJob
- [ ] McJobPanel with dry-run/execute toggle
- [ ] Post-job rescan + DB update

### Phase 4 — Track Classifier
- [ ] RegexClassifier + data/track_classifier.json
- [ ] Commentary/SDH/forced handling in rule engine
- [ ] OnnxClassifier (optional, on-demand download)
- [ ] ApiClassifier (optional, user provides Claude API key)

### Phase 5 — Non-MKV + Packaging
- [ ] Non-MKV remux flow (MP4/AVI → MKV in one pass with track filtering)
- [ ] CPack: NSIS installer (Windows), DMG (macOS), AppImage (Linux)
- [ ] scripts/setup_tools.ps1 (auto-download ffprobe + mkvmerge)
- [ ] About dialog with donation link
- [ ] GitHub Actions CI

---

## Non-MKV File Strategy
- MP4, M4V, AVI, WMV, M2TS: fully supported via mkvmerge input
- Workflow: "Convert & Clean" — remux to MKV applying track policy in a single pass
- Original file kept until user confirms deletion (trash / explicit delete)
- Encrypted / DRM'd files: flagged as read-only in UI, scan metadata only

---

## External Tool Bundling
Tools are placed in `tools/` at runtime (not in source tree):
```
tools/
  windows/
    ffprobe.exe       ← from gyan.dev LGPL builds
    mkvmerge.exe      ← from MKVToolNix installer/portable
    mkvpropedit.exe
  linux/
    ffprobe
    mkvmerge
    mkvpropedit
  macos/
    ffprobe
    mkvmerge
    mkvpropedit
```
`scripts/setup_tools.ps1` (Windows) and `scripts/setup_tools.sh` (Linux/macOS)
download the correct builds automatically.
