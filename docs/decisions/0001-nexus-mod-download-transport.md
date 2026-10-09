# ADR-0001: Use the Nexus v1 download-link endpoint with a local Qt transport

## Status

Accepted

## Date

2026-10-08

## Context

Prismatic Launcher needs a direct, client-side way to download a selected
Nexus Mods file. Nexus's current v3 OpenAPI schema is the active API, but its
published file operations are metadata-oriented and marked experimental; it
does not publish a client download-link operation. Nexus's official client
documentation still describes `getDownloadURLs`, and its legacy v1 API exposes
the corresponding `download_link` endpoint.

Nexus API keys identify a user and an application. Nexus explicitly says an
application must not reuse an API key created for another application. The
application is not yet registered for SSO, and a shared embedded credential
would violate that restriction.

## Decision

Use the v1 `GET /v1/games/{game}/mods/{mod}/files/{file}/download_link.json`
endpoint to resolve a short-lived CDN URL, authenticating with a user-owned key
from `NEXUSMODS_API_KEY`. The command accepts an optional download key and
expiry for download scenarios where Nexus requires them.

The `NexusHttpTransport` interface separates Nexus response validation from Qt
network I/O. The Qt implementation sends only HTTPS requests, permits only
redirects that are no less safe, streams to `QSaveFile`, imposes a caller-set
size cap, and commits the output only after a successful transfer.

## Alternatives considered

### Nexus v3 only

- Pros: current API generation.
- Cons: no published client download-link operation in the schema at the time
  of this decision.
- Rejected for this download slice; v3 can be adopted later for metadata when
  its download flow is documented.

### Embedded shared API key

- Pros: no user setup.
- Cons: violates Nexus's key-ownership guidance and exposes a revocable secret.
- Rejected.

### Download in memory before writing

- Pros: simple test double.
- Cons: unsuitable for large mod archives and leaves less control over partial
  file handling.
- Rejected.

## Consequences

- Users must create and supply their own Nexus API key, and the app must be
  registered before a production SSO experience is added.
- The v1 endpoint is a deliberate compatibility dependency and should be
  revisited when Nexus documents a v3 client download operation.
- The integration does not install or inspect downloaded archives.

## Sources

- https://github.com/Nexus-Mods/node-nexus-api/blob/master/docs/README.md
- https://github.com/Nexus-Mods/node-nexus-api/blob/master/docs/classes/_nexus_.nexus.md
- https://github.com/Nexus-Mods/Vortex/blob/master/packages/nexus-api-v3/schema/openapi.yaml
