# MediaVault

A cross-platform (Windows, macOS, Linux) media player for movies, TV shows and music, written in C++17 with Qt 6.

Point it at one folder and it builds a browsable library from the three folders inside:

```
Media/
├── Movies/
│   ├── The Matrix (1999).mkv
│   └── Alien (1979)/Alien (1979).mp4
├── TV Shows/
│   └── Breaking Bad/
│       └── Season 1/Breaking.Bad.S01E01.mkv
└── Music/
    └── Pink Floyd/
        └── The Wall/
            └── 01 - In The Flesh.flac
```

Folder names are matched case-insensitively. `TV`, `TVShows` and `Series` also work for the TV folder.

## Using it

- **File → Open Media Folder…** (Ctrl/Cmd+O), or the **⋯** menu in Classic, and pick the folder that *contains* Music, Movies and TV Shows. The choice is remembered.
- Or from a terminal: `MediaVault /path/to/Media`
- Click an item to play it (double-click in the Legacy layout). Playback moves on to the next item automatically.
- Type in the search box to narrow what you see.
- Keys: **Space** play/pause, **←/→** seek 10s, **F11** or double-click the video for fullscreen (**Esc** exits; controls hide after 3s and return when you move the mouse), **Ctrl/Cmd+R** rescan.

### How files are recognised

| Type | Layout | Notes |
|------|--------|-------|
| Movies | any depth under `Movies/` | Title comes from the file name |
| TV | `TV Shows/<Show>/…/<episode>` | Season/episode from `S01E02` or `1x02`, otherwise from a `Season N` folder |
| Music | `Music/<Artist>/<Album>/<track>` | Leading track numbers are stripped; loose files go under "Unknown Artist" or "Singles" |

Formats depend on the Qt Multimedia backend (FFmpeg on Qt 6.5+), which covers mkv, mp4, avi, mov, webm, mp3, flac, ogg, m4a, opus, wav and more.

## Discord status

MediaVault can show what you're watching or listening to on Discord: title, show/season/episode or artist/album, a progress bar, and a picture. It talks to your local Discord desktop app, so Discord must be running.

1. Create an application at <https://discord.com/developers/applications>. Its **name** is what Discord displays ("Watching *MediaVault*"), so name it accordingly.
2. Copy its **Application ID**.
3. In MediaVault: **File → Show on Discord**, and paste the ID when asked. Everything below lives in **File → Discord Settings…**.
4. In Discord, make sure *User Settings → Activity Privacy → Share my activity* is on.

### Picture

- **No picture**
- **My own image**: upload a picture in the developer portal under *Rich Presence → Art Assets*, then enter the name you gave it (for example `logo`) as the Image ID. A direct `https://` image link also works.
- **Cover art and posters**: album covers come from MusicBrainz and the Cover Art Archive (no key needed). Movie and show posters come from [TMDB](https://www.themoviedb.org/settings/api), which needs a free API key that you paste into the dialog. Titles are guessed from file and folder names, so `The.Matrix.1999.1080p.mkv` and `Alien (1979)` both work. Your own Image ID is used as the fallback whenever nothing is found.

The status clears when you stop playback or quit. If Discord isn't running yet, MediaVault retries every 15 seconds.

## Themes and layouts

MediaVault's window is a theme. The default, **Classic**, is a web UI that fills the whole window: Music, Movies and TV Shows tabs, search, and a player bar. Songs play inside it; movies and episodes play on a native video screen with a **Library** button to get back.

**View → Theme** switches themes. The last entry, **Legacy (media browser on the left)**, is the original layout: library list beside the player. A theme can also be just a Qt style sheet and fonts, or a music-only panel for the Legacy layout. Put your own in **View → Theme → Open Themes Folder**.

Album covers are read from `cover.jpg` / `folder.jpg` / `front.jpg` (also `.png`, `.webp`) in the album folder, and artist pictures from `artist.jpg` / `folder.jpg` in the artist folder.

How to make a theme: [docs/THEMES.md](docs/THEMES.md). Web themes need a build with Qt WebEngine (the CI builds include it); without it MediaVault falls back to the Legacy layout.

## Building

Requires CMake 3.21+, a C++17 compiler and Qt 6.5+ with the Multimedia module. Web themes also need the WebEngine and WebChannel modules; without them the app still builds (`-DMEDIAVAULT_WEBENGINE=OFF` to skip them on purpose) and just applies a theme's style sheet.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build        # scanner tests
./build/MediaVault /path/to/Media
```

Ubuntu/Debian: `sudo apt install qt6-base-dev qt6-multimedia-dev libqt6multimediawidgets6 cmake g++`

## GitHub Actions

`.github/workflows/build.yml` builds and tests on Linux, Windows and macOS for every push and pull request, and uploads the results as artifacts (Windows is bundled with `windeployqt`, macOS with `macdeployqt`). Pushing a tag such as `v0.1.0` also publishes a GitHub Release with those builds attached.

## Roadmap ideas

Metadata/artwork (TMDB, embedded tags), subtitle and audio track selection, watched/resume tracking, a music queue and shuffle, gapless playback.
