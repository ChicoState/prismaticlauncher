# Nexus mod browser

Choose **Browse Nexus mods** in the library, or open an instance and choose
**Mods**, to browse Stardew Valley mods, sorted by most recently updated.
Each page loads up to **50 mods**. Use **Previous** and **Next** below the list;
the result counter shows the displayed range and the total available on Nexus.
Filter the current page by name, author, or summary; the filter is local to that
page and remains active when navigating. It is not a search of the full catalogue.
**Refresh** returns to the first page. Page controls are disabled during requests;
a failed request keeps the current list and selected details visible so you can
retry the same navigation. Click a name or use the arrow keys to
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
launcher/config tests. Dedicated `nexus-mod-pages-test` and `mod-pagination-tests`
targets cover page parsing, POST parameters, invalid responses and GraphQL errors,
first/last pages, duplicate requests while loading, retry after HTTP errors, and
selection handling across refreshes. An asynchronous fixture transport keeps all
automated API and UI tests offline.

To save real widget renders using explicitly synthetic mod fixtures:

```sh
PRISMATIC_SCREENSHOTS=/tmp/prismatic-preview ctest --preset dev --output-on-failure
```

`nexus-selected-sample.png`, `mods-selected-sample.png`,
`mods-minimum-sample.png`, and `mods-pagination-sample.png` are fixture previews,
not live Nexus results. Live API authentication and OS browser launching should
also be smoke-tested on the supported native platforms before release.

## Visual source

The browser is integrated with the Designer-based launcher and separate instance
window from `main`. It uses the same light gray canvas, white surfaces, 4px panel
corners, amber list selections, blue download action, and compact controls.
The instance Mods tab loads the list on first entry and preserves selection when
the window is closed and reopened. The library toolbar also offers standalone
Nexus browsing with a back button.

## API choice

The UI uses the [Nexus GraphQL `mods` query](https://graphql.nexusmods.com/#query-mods)
with `offset`/`count`, scoped to `stardewvalley` and sorted by `updatedAt` descending.
The previous REST `latest_updated.json` feed only supplies ten mods and cannot
page through the catalogue. The CLI probe retains that legacy feed; selected mod
details still use REST. No new dependencies, credentials, or stored secrets are added.

Nexus labels GraphQL v2 as work in progress. Responses are validated and GraphQL
errors are rejected even with HTTP 200. Offset pages reflect a live catalogue:
mods updated while browsing can move between pages, so refresh to restart from
the newest results. The optional injected network manager is borrowed and must
outlive the client/page; production creates a manager owned by each client.
