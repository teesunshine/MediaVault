# Making a MediaVault theme

A theme can do two things, separately or together:

- **Web panel**: an HTML page shown in place of the video while you're on **music**. It receives what's playing and can control playback. Clicking an artist or album in the library opens its page here.
- **App style**: a Qt style sheet (`.qss`) and fonts that restyle the whole window (library list, buttons, menus).

Pick one in **View → Theme**. **View → Theme → Open Themes Folder** opens the folder where your own themes go; bundled themes live in a `themes` folder next to the app. A theme in your folder with the same `id` as a bundled one replaces it. After editing a theme, use **Reload Themes**.

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
  "qss": "app.qss",
  "fonts": ["fonts/Some.ttf"]
}
```

Everything except `name` is optional, but a theme needs `entry` and/or `qss`. `entry` defaults to `index.html` if that file exists. Paths are relative to the theme folder and can't point outside it.

## The web page

Your page is an ordinary local web page. Before your scripts run, MediaVault adds a global `MediaVault` object. Pages are loaded from disk and cannot load remote (`http`/`https`) resources, so bundle your fonts and images.

### Events

```js
MediaVault.on('ready',   state => { ... });   // once, when the bridge is connected
MediaVault.on('state',   state => { ... });   // whenever anything changes (about twice a second while playing)
MediaVault.on('library', ()    => { ... });   // the library was rescanned: call getLibrary() again
```

`on('state', fn)` calls `fn` immediately if a state is already known. The same events are also dispatched on `window` as `mediavault:state`, `mediavault:ready` and `mediavault:library` (`event.detail` is the state).

`MediaVault.state` always holds the latest state:

```js
{
  kind: "music" | "video" | "none",
  title: "In The Flesh",
  artist: "Pink Floyd",           // music only
  album: "The Wall",              // music only
  cover: "file:///.../cover.jpg", // "" if the album folder has no cover image
  playing: true,
  position: 62000,                // milliseconds
  duration: 199000,
  volume: 80,                     // 0..100
  view: {                         // what the user asked to see
    type: "nowplaying" | "artist" | "album",
    artist: "Pink Floyd",         // for "artist" and "album"
    album: "The Wall"             // for "album"
  }
}
```

`view` changes when the user clicks an artist or album in the library list (or double-clicks a song, which sets it back to `nowplaying`). Render whatever it says.

### Library

```js
const lib = await MediaVault.getLibrary();
// { artists: [ { name, image, albums: [ { name, cover, tracks: [ { title, path } ] } ] } ] }
```

`image` and `cover` are `file://` URLs, or `""` if there's none. Covers are picked up from an image named `cover`, `folder`, `front`, `album` or `artwork` (`.jpg`, `.png`, `.webp`) in the album folder; artist images from `artist`, `folder`, `photo` or `band` in the artist folder. `path` is what you pass to `playTrack`.

### Commands

```js
MediaVault.play();            MediaVault.pause();         MediaVault.togglePlay();
MediaVault.next();            MediaVault.previous();
MediaVault.seek(ms);          MediaVault.setVolume(0-100);
MediaVault.playTrack(path);
MediaVault.openArtist(name);  MediaVault.openAlbum(artist, album);
MediaVault.showNowPlaying();  MediaVault.toggleFullscreen();
```

`openArtist`, `openAlbum` and `showNowPlaying` change `state.view` (and trigger a `state` event), so your page doesn't need to keep its own navigation history if it doesn't want to.

### Developing without the app

Open your `index.html` in a browser and add a stub before your script, for example:

```html
<script>
  window.MediaVault = window.MediaVault || {
    on(name, fn) { if (name === 'state') fn({kind:'music', title:'Test', artist:'A', album:'B', cover:'',
      playing:true, position:1000, duration:200000, volume:80, view:{type:'nowplaying'}}); return this; },
    getLibrary: async () => ({ artists: [] }),
    togglePlay(){}, next(){}, previous(){}, seek(){}, setVolume(){}, playTrack(){},
    openArtist(){}, openAlbum(){}, showNowPlaying(){}, toggleFullscreen(){}
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

- Web themes need a build with Qt WebEngine. Builds without it still apply `qss` and fonts.
- A theme page can only talk to MediaVault through the `MediaVault` object above.
- Don't put other people's artwork, fonts or audio in a theme you publish unless you have the right to share them.
