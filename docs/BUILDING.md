## Building MediaCurator

### Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| Visual Studio | 2026 | "Desktop development with C++" workload |
| Qt | 6.8.3 LTS | MSVC 2022 64-bit component via Qt Maintenance Tool |
| CMake | 3.25+ | Ships with VS 2026, or install standalone |

There is no external package manager. `nlohmann/json` 3.11.3 is vendored at `third_party/nlohmann/json.hpp`.

---

### Windows (Visual Studio 2026)

#### Step 1 — Clone

```powershell
git clone https://github.com/yourusername/MediaCurator.git
cd MediaCurator
```

#### Step 2 — Create CMakeUserPresets.json

This file is gitignored (local only). Create it in the repo root and adjust the Qt path to match your install:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-release",
      "inherits": "release",
      "environment": {
        "Qt6_DIR": "D:/Development/Environment/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6"
      }
    },
    {
      "name": "local-debug",
      "inherits": "debug",
      "environment": {
        "Qt6_DIR": "D:/Development/Environment/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6"
      }
    }
  ]
}
```

#### Step 3 — Download tool binaries (one-time)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup_tools.ps1
```

This places `ffprobe.exe` and `mkvmerge.exe` in `tools/windows/`.

#### Step 4 — Configure and build

```powershell
cmake --preset local-release
cmake --build --preset local-release
```

Or open `build/MediaCurator.sln` in Visual Studio 2026 and build from the IDE.

#### Step 5 — Run

```
build/bin/Release/MediaCurator.exe
```

---

### macOS / Linux

Use the `release` preset (no `CMakeUserPresets.json` needed if Qt is on `PATH` or in a standard location):

```bash
# Set Qt path if not auto-detected
export Qt6_DIR=/path/to/Qt/6.8.3/gcc_64/lib/cmake/Qt6

cmake --preset release
cmake --build --preset release
```

---

### Common issues

**Qt6 not found**

Set `Qt6_DIR` in `CMakeUserPresets.json` (Windows) or as an env var (macOS/Linux). Typical paths:

- Windows: `D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6`
- macOS: `~/Qt/6.8.3/macos/lib/cmake/Qt6`
- Linux: `~/Qt/6.8.3/gcc_64/lib/cmake/Qt6`

**QSQLITE driver not found at runtime**

The `CMakeLists.txt` POST_BUILD step runs `windeployqt` automatically if it is on `PATH`. If the driver is still missing, copy `sqldrivers/qsqlite.dll` next to the executable.

**Tools directory empty**

Run `scripts/setup_tools.ps1` (Windows) or manually place `ffprobe` and `mkvmerge` in `tools/`.
