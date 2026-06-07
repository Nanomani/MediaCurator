# MediaCurator

**MediaCurator** is a cross-platform desktop application that helps you reclaim hard drive space by intelligently stripping unwanted audio tracks and subtitle streams from your movie collection — without re-encoding the video.

## What It Does

Modern movie files often ship with 8–15 audio tracks covering every language used in distribution, plus multiple subtitle streams, commentary tracks, and secondary mixes. If you only watch films in one or two languages, all of those extra streams are dead weight. A typical Blu-ray rip can carry 2–3 GB of audio tracks you will never use.

MediaCurator scans your library, identifies which tracks you actually want, and queues up lossless remux operations (via **mkvmerge**) to remove the rest. Because the video stream is never touched, there is zero quality loss and processing is fast — typically a few seconds per file.

**Practical savings:** removing a 5.1 DTS-HD MA English track plus a Dolby Atmos track from a 4K Blu-ray rip can save 2–4 GB per file. For a library of 200 films that is 400–800 GB — the equivalent of a mid-size NAS drive.

## Features

- **Library scanning** — Point MediaCurator at a folder and it catalogues every video file with full stream information (codec, language, channels, resolution, HDR format).
- **Smart rule engine** — Configurable rules decide which tracks to keep. Examples: keep any English audio, keep the highest-quality track regardless of language, never remove the only audio track, always keep forced subtitles.
- **File card view** — Each file is shown as a card with colour-coded badges for video, audio, and subtitle streams. Streams proposed for removal are shown with a strikethrough. Click any badge to manually override the rule decision.
- **Job queue** — Proposed removals are queued as jobs. Review them before running. Cancel individual jobs or the whole queue.
- **Poster art** — Posters are fetched from TMDB automatically when an NFO/IMDb ID is available, or you can double-click the poster column to search and link a movie. Posters are cached locally.
- **IMDb / TMDB integration** — Search TMDB by title, browse results with poster thumbnails, and save the IMDb ID to a Kodi-compatible NFO file alongside the video.
- **VLC integration** — Play any file directly from the library view.
- **Dark mode support** — Uses the system colour scheme; icons and UI elements adapt automatically.

## Why Disk Space Matters

Hard drives are not cheap. A 16 TB NAS drive for home media storage costs upwards of $300. Reducing each movie's size by 10–30% directly translates to a proportionally larger library on the same hardware — or a lower storage bill overall.

## Requirements

**The installer bundles all required tools** — no separate downloads needed.

| Tool | Role | Notes |
|------|------|-------|
| ffprobe | Stream analysis | Bundled |
| mkvmerge | Lossless remux | Bundled |
| VLC | In-app playback | Optional — auto-detected if installed |

For a TMDB API key (free), visit [themoviedb.org/settings/api](https://www.themoviedb.org/settings/api). Enter it in **Settings** to enable poster art and movie search. The app works without it.

### Platform support

| Platform | Status |
|----------|--------|
| Windows 10 / 11 | Primary — fully tested |
| macOS 12+ | Builds and runs; less tested |
| Linux (x64) | Builds and runs; less tested |

## Building from Source

MediaCurator is written in C++20 with Qt 6.8+. Dependencies are managed via vcpkg.

```powershell
# Windows
cmake --preset local-release
cmake --build --preset local-release
```

```bash
# macOS / Linux
cmake --preset release
cmake --build --preset release
```

See `CLAUDE.md` for detailed environment setup (Qt paths, vcpkg, compiler).

After building, place the tool binaries alongside the executable:

```
<build output>/
  MediaCurator.exe
  tools/
    windows/
      ffprobe.exe
      mkvmerge.exe
```

See `scripts/setup_tools.ps1` for a script that downloads and places these automatically.

## License

Licensed under the [Apache License 2.0](LICENSE).

## Donate

If MediaCurator saves you money on storage or just makes managing your media library less painful, consider saying thanks or supporting continued development via:

- [GitHub Sponsors](https://github.com/sponsors/your-username)
- [Buy Me a Coffee](https://buymeacoffee.com/your-username)
- ⚡ Lightning: `bleze@cake.cash`

## Roadmap

- Subtitle search and download (OpenSubtitles integration)
- Poster and backdrop browser with Plex-compatible export
- Backdrop / fanart fetching
- Remove folder from library
