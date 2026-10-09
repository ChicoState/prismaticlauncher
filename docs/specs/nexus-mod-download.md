# Spec: Nexus Mods file download

## Objective

Provide a small command-line integration that asks Nexus Mods for a download URL
for a selected mod file and saves the resulting archive locally. It is an
integration foundation only: it does not discover games, select a mod file,
extract archives, install a mod, or persist credentials.

The command succeeds only when a valid Nexus response supplies an HTTPS URL and
the archive is written atomically to the requested destination.

## Tech stack

- C++20 and Qt 6.8 (`Core`, `Network`, and `Test`)
- Nexus Mods public API v1 `download_link` endpoint
- A user-owned Nexus API key supplied at runtime in `NEXUSMODS_API_KEY`

## Commands

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
NEXUSMODS_API_KEY='...' ./build/dev/prismatic-nexus-download \
  --game stardewvalley --mod 2400 --file 12345 --output ./downloads/mod.zip
```

## Project structure

- `prismatic-launcher/src/nexus/` — typed download request/result contracts,
  Nexus URL resolution, and Qt network transport.
- `prismatic-launcher/src/main.cpp` — intentionally small CLI boundary.
- `tests/nexus/` — Qt Test unit tests with a deterministic fake transport.
- `docs/decisions/` — durable rationale for the selected integration.

## Code style

```cpp
NexusDownloadResult result = downloader.download(request);
if (!result.succeeded()) {
    return ExitFailure;
}
```

Public interfaces use explicit request and result types. External JSON is
validated before use; errors are stable enum values paired with safe,
human-readable messages. No secret is included in an error or log message.

## Testing strategy

Small Qt Test cases use a fake HTTP transport; they do not contact Nexus Mods
or require a key. Tests cover malformed input, missing credentials, unsafe or
malformed API responses, API failures, and a successful download handoff.

## Boundaries

- Always: require HTTPS, validate external JSON and all CLI input, use a
  user-supplied API key only at runtime, cap download size, and use atomic file
  output.
- Ask first: persist credentials, register an OAuth/SSO application, add
  archive extraction or mod installation, or add a new dependency.
- Never: commit, log, or accept an API key as a command-line argument; follow
  a non-HTTPS redirect; or write an incomplete archive to the final file name.

## Success criteria

- The build creates `prismatic-nexus-download` and `nexus-download-tests`.
- The CLI accepts a game domain, mod ID, file ID, destination, and optional
  Nexus download key/expiry; its API key comes only from the environment.
- A successful Nexus link response leads to a bounded, streamed, atomic file
  download.
- Non-HTTPS links, malformed API JSON, invalid input, missing API keys, and
  failed transfers produce a defined failure without leaving a partial output.
- `ctest --preset dev --output-on-failure` passes without network access.

## Open questions

- Nexus application registration, SSO, and encrypted local credential storage
  are intentionally deferred. This setup uses a user-generated API key.
- Non-premium downloads can require an `nxm` download key and expiry obtained
  from Nexus Mods; selecting or opening that link is outside this slice.
