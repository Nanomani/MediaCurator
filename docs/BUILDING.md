## Building MediaCurator on Windows (Visual Studio 2026)

### Prerequisites
1. **Visual Studio 2026** with "Desktop development with C++" workload
2. **Qt 6.7+** installed via Qt Maintenance Tool
   - During install, select: MSVC 2022 64-bit component
3. **CMake 3.25+** (ships with VS 2026, or install standalone)
4. **vcpkg** (for nlohmann/json)
5. External tools: run `scripts/setup_tools.ps1` once after clone

---

### Step 1 — Clone and set up
```powershell
git clone https://github.com/yourusername/MediaCurator.git
cd MediaCurator
powershell -ExecutionPolicy Bypass -File scripts/setup_tools.ps1
```

### Step 2 — Bootstrap vcpkg (one-time)
```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg integrate install
```

### Step 3 — Configure (adjust Qt path to match your install)
```powershell
cmake -B build `
  -G "Visual Studio 17 2022" -A x64 `
  -DQt6_DIR="C:/Qt/6.7.2/msvc2022_64/lib/cmake/Qt6" `
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake"
```

### Step 4 — Open in Visual Studio 2026
```powershell
start build/MediaCurator.sln
```
Or build from the command line:
```powershell
cmake --build build --config Release
```

### Step 5 — Run
```
build/bin/Release/MediaCurator.exe
```

---

### Common issues

**Qt6 not found**
Set `Qt6_DIR` explicitly as shown above. Common paths:
- `C:/Qt/6.7.2/msvc2022_64/lib/cmake/Qt6`
- `C:/Qt/6.8.0/msvc2022_64/lib/cmake/Qt6`

**QSQLITE driver not found at runtime**
Run `windeployqt` on the output exe, or copy `sqldrivers/qsqlite.dll` next to the exe.
The CMakeLists.txt does this automatically via the POST_BUILD step if `windeployqt` is in PATH.

**nlohmann_json not found**
Make sure you passed `-DCMAKE_TOOLCHAIN_FILE=vcpkg/...` to cmake and vcpkg is bootstrapped.

**Tools directory empty**
Run `scripts/setup_tools.ps1` first, or manually place ffprobe.exe and mkvmerge.exe
in `tools/windows/`.
