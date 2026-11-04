# Making a MediaVault theme

A theme can do these things, separately or together:

- **Be the whole app.** Its HTML page fills the window: library, browsing, search, player controls, everything. This is what the bundled **Classic** theme does and it is the default. Video is drawn by the app over a region your page reserves, so your own top and bottom bars stay around it (see below).
- **Be a music panel** (`"layout": "panel"`). The older, smaller idea: the app keeps the *Legacy* layout (library list on the left) and your page replaces the video area while you're on music.
- **Restyle the app**: a Qt style sheet (`.qss`) and fonts that restyle the native parts (the Legacy layout, the video screen's controls, menus).

Pick one in **View → Theme**. The last entry, **Legacy (media browser on the left)**, is the original layout with the library list beside the player and no web page at all. **Open Themes Folder** opens the folder for your own themes; bundled themes live in a `themes` folder next to the app. A theme in your folder with the same `id` as a bundled one replaces it. After editing a theme, use **Reload Themes**.

If a build has no Qt WebEngine, web themes can't run and MediaVault falls back to the Legacy layout.

## Layout

```
my-theme/
├── theme.json
├── index.html        (web panel, optional)
├── app.qss           (app style, optional)
├── fonts/Some.ttf    (optional)
└── anything else your page uses: images, css, js
```

`theme.json`:

```json
{
  "id": "my-theme",
  "name": "My Theme",
  "author": "you",
  "version": "1.0",
  "description": "Shown as a tooltip in the menu",
  "entry": "index.html",
  "layout": "full",
  "qss": "app.qss",
  "fonts": ["fonts/Some.ttf"]
}
```

Everything except `name` is optional, but a theme needs `entry` and/or `qss`. `entry` defaults to `index.html` if that file exists. `layout` is `"full"` (default: the page is the whole app) or `"panel"` (music panel inside the Legacy layout). Paths are relative to the theme folder and can't point outside it.

## The web page

Your page is an ordinary local web page. Before your scripts run, MediaVault adds a global `MediaVault` object. Pages are loaded from disk and cannot load remote (`http`/`https`) resources, so bundle your fonts and images.

### Events

```js
MediaVault.on('ready',   state => { ... });   // once, when the bridge is connected
MediaVault.on('state',   state => { ... });   // whenever anything changes (about twice a second while playing)
MediaVault.on('library', ()    => { ... });   // the library was rescanned: call getLibrary() again
MediaVault.on('status',  text  => { ... });   // a message the app's status bar would show (errors, Discord)
MediaVault.on('activity', ()   => { ... });   // the mouse moved while the window is fullscreen
```

`on('state', fn)` calls `fn` immediately if a state is already known. The same events are also dispatched on `window` as `mediavault:state`, `mediavault:ready` and `mediavault:library` (`event.detail` is the state).

`MediaVault.state` always holds the latest state:

```js
{
  kind: "music" | "video" | "none",
  title: "In The Flesh",          // song, movie, or episode title
  artist: "Pink Floyd",           // music only
  album: "The Wall",              // music only
  show: "Breaking Bad",           // TV only: the show's name
  season: 1, episode: 2,          // TV only
  path: "/media/Music/...",       // file that's loaded, "" if nothing is
  cover: "file:///.../cover.jpg", // music: "" if the album folder has no cover image
  playing: true,
  position: 62000,                // milliseconds
  duration: 199000,
  volume: 80,                     // 0..100
  page: "ui" | "video",           // full UI: "video" while your page has reserved the video region
  fullscreen: false,              // the window is fullscreen
  root: "/media",                 // the media folder, "" until the user picks one
  view: {                         // panel themes only: what the user asked to see
    type: "nowplaying" | "artist" | "album",
    artist: "Pink Floyd",         // for "artist" and "album"
    album: "The Wall"             // for "album"
  }
}
```

In a **panel** theme, `view` changes when the user clicks an artist or album in the Legacy library list (or double-clicks a song, which sets it back to `nowplaying`); render whatever it says. A **full** theme does its own navigation and can ignore `view`.

### Library

```js
const lib = await MediaVault.getLibrary();
// {
//   root: "/media",
//   movies:  [ { title, path } ],
//   shows:   [ { name, seasons: [ { number, episodes: [ { title, season, episode, path } ] } ] } ],
//   artists: [ { name, image, albums: [ { name, cover, tracks: [ { title, path } ] } ] } ]
// }
```

Everything is sorted for display. `number`, `season` and `episode` are `0` when they couldn't be worked out. `image` and `cover` are `file://` URLs, or `""` if there's none. Covers are picked up from an image named `cover`, `folder`, `front`, `album` or `artwork` (`.jpg`, `.png`, `.webp`) in the album folder; artist images from `artist`, `folder`, `photo` or `band` in the artist folder. `path` is what you pass to `playMedia`.

### Commands

```js
MediaVault.play();            MediaVault.pause();         MediaVault.togglePlay();
MediaVault.next();            MediaVault.previous();
MediaVault.stop();
MediaVault.seek(ms);          MediaVault.setVolume(0-100);
MediaVault.playMedia(path);   // any song, movie or episode (playTrack is the same thing)
MediaVault.openFolder();      // asks the user for the media folder
MediaVault.rescan();          // rescans it; you get a "library" event afterwards
MediaVault.showVideo();       MediaVault.showLibrary();   // full UI: switch between video and library
MediaVault.bindVideo(el);     MediaVault.unbindVideo();   // full UI: where to draw the video
MediaVault.toggleFullscreen();

// Panel themes only:
MediaVault.openArtist(name);  MediaVault.openAlbum(artist, album);  MediaVault.showNowPlaying();
```

`openArtist`, `openAlbum` and `showNowPlaying` change `state.view` (and trigger a `state` event); full themes don't need them.

### Video

A web page can't draw video itself, so the app draws it *over* your page. Reserve a region with an element and bind it:

```js
MediaVault.on('state', s => {
  const onVideo = s.page === 'video';
  slot.hidden = !onVideo;
  if (onVideo) MediaVault.bindVideo(slot); else MediaVault.unbindVideo();
});
```

While bound, the video is placed exactly over the element (its size is re-checked about ten times a second and on window resize). Your top bar, player bar and anything else outside it remain your page, so the video matches your theme. Nothing you draw *inside* the region is visible. `playMedia` on a movie or episode sets `state.page` to `"video"`; songs keep it `"ui"`. Use `showVideo()` / `showLibrary()` to switch, and offer a way back to the video when `state.kind === "video"` and `state.page === "ui"`.

In fullscreen (`state.fullscreen`), bars can't float over the video, so hide them (the Classic theme does after 3 seconds) and bring them back on the `activity` event or your own `mousemove`. Esc leaves fullscreen. The app's own status bar is hidden in a full UI; show the `status` messages in your own.

### Developing without the app

Open your `index.html` in a browser and add a stub before your script, for example:

```html
<script>
  window.MediaVault = window.MediaVault || {
    on(name, fn) { if (name === 'state') fn({kind:'music', title:'Test', artist:'A', album:'B', cover:'',
      playing:true, position:1000, duration:200000, volume:80, view:{type:'nowplaying'}}); return this; },
    getLibrary: async () => ({ root: '', movies: [], shows: [], artists: [] }),
    togglePlay(){}, next(){}, previous(){}, seek(){}, setVolume(){}, playMedia(){}, playTrack(){},
    openFolder(){}, rescan(){}, showVideo(){}, showLibrary(){}, toggleFullscreen(){}, bindVideo(){}, unbindVideo(){},
    openArtist(){}, openAlbum(){}, showNowPlaying(){}
  };
</script>
```

In the app the real object is added before your page loads, so the `||` keeps yours from interfering.

## The app style

`app.qss` is a [Qt style sheet](https://doc.qt.io/qt-6/stylesheet-reference.html). Write `{{theme}}` where you need the theme folder's path:

```css
QWidget { background-color: #000; color: #accbf1; }
QTreeWidget { background-image: url({{theme}}/images/bg.png); }
QSlider::handle:horizontal { background: #accbf1; }
```

Fonts listed in `theme.json` are registered while the theme is active and can be used by their family name (`font-family: "Bank Gothic";`).

## Notes

- Web themes need a build with Qt WebEngine. Builds without it use the Legacy layout and still apply `qss` and fonts.
- The bundled `themes/classic` is a complete working example of a full theme.
- A theme page can only talk to MediaVault through the `MediaVault` object above.
- Don't put other people's artwork, fonts or audio in a theme you publish unless you have the right to share them.
