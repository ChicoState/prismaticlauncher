# Nexus mod browser

Choose **Browse Nexus mods** in the library, or open an instance and choose
**Mods**, to browse the ten most recently updated Stardew Valley mods. Filter the fetched list by name, author, or summary; this is
not a search of the full Nexus catalogue. Click a name or use the arrow keys to
show its summary and full details. The bottom-right **Open download page** button
opens that mod's Nexus Files tab in the default browser. It does not download or
install files. Unavailable mods have a disabled download button.
HTML and Nexus BBCode descriptions are converted to readable plain text, with
paragraph/line breaks, decoded HTML entities, and bullets for BBCode lists.
Descriptions already in plain text retain their line breaks.

## Run

Build with the existing Qt 6/CMake prerequisites, then supply your own API key via
the process environment before launching `build/dev/bin/prismatic-launch`.
The launcher reads `NEXUS_API_KEY` at startup; it does not save it in settings or
display it. Do not commit keys or put them in screenshots. Without a key the page
shows setup guidance, while the rest of the launcher remains usable.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## UI verification

The `launcher-ui-tests` CTest target runs without a key or external network.
It checks mouse/keyboard selection, search, loading/error/empty states, stale
detail responses and cancellation, unavailable mods, browser URL dispatch, back
navigation, and the 820 × 600 minimum window size. It also retains the existing
launcher/config tests.

To save real widget renders using explicitly synthetic mod fixtures:

```sh
PRISMATIC_SCREENSHOTS=/tmp/prismatic-preview ctest --preset dev --output-on-failure
```

`nexus-selected-sample.png`, `mods-selected-sample.png`, and
`mods-minimum-sample.png` are fixture previews, not live Nexus results. Live API authentication and OS browser launching should also
be smoke-tested on the supported native platforms before release.

## Visual source

The browser is integrated with the Designer-based launcher and separate instance
window from `main`. It uses the same light gray canvas, white surfaces, 4px panel
corners, amber list selections, blue download action, and compact controls.
The instance Mods tab loads the list on first entry and preserves selection when
the window is closed and reopened. The library toolbar also offers standalone
Nexus browsing with a back button.
