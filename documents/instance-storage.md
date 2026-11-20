# Directory-based instances

The launcher stores each instance in `<configuration basename>.instances/<instance name>/`, next to its INI configuration. For example, `--config /data/launcher.ini` uses `/data/launcher.instances/My farm/`. The normal location is beside `launcher.ini` in Qt's `AppConfigLocation`. Separate configuration basenames have separate libraries.

Each instance contains `instance.json` (schema version 1, name, and modded boolean) and `Mods/`. Folder names exactly match accepted user names, including spaces and Unicode. Names are immutable in this release; they identify records. Blank names, leading/trailing whitespace, dot-prefixed names, path separators, control characters, Windows-forbidden characters/device names, trailing periods, and names over 200 UTF-8 bytes are rejected. Case-insensitive, Unicode-normalized collisions with existing files or folders are rejected rather than renamed or overwritten.

New libraries start with one saved Stardew Valley vanilla instance. Existing empty libraries stay empty. New instances publish from same-filesystem staging directories. Metadata changes use QSaveFile with direct-write fallback disabled and are committed before UI success. A short-lived library lock serializes writes. Existing windows do not automatically refresh changes from other application processes.

Startup loads healthy instance directories and reports malformed, linked, or unsupported records without replacing them. Missing roots are initialized; unreadable roots are errors. Linked instance directories, metadata files, and Mods directories are rejected. Interrupted hidden staging directories are ignored and retained for manual recovery. No SQLite dependency or database migration is involved.

The configured game installation supplies executables and SMAPI. Installed mods and mod configuration files live under the selected instance's Mods directory; installations retain the existing validate/stage/no-overwrite behavior. Modded launches pass that absolute path to SMAPI using `--mods-path`; vanilla launches use the normal game executable. Existing game-folder mods are not copied or changed. Game saves and mods that store data outside their own directories are not isolated.

Back up an instance by copying its complete folder while the app and game are closed. Logs remain session-only. Renaming, deleting, exporting, mod replacement, isolated game copies, and automatic metadata upgrades are outside this feature.

## Verification contract

Qt tests cover restart persistence, exact user-named folders, mode edits bound to the detail instance, unsafe and colliding names, malformed/future metadata preservation, link rejection, failed writes, per-instance mod installation/listing, and SMAPI arguments. Tests use temporary libraries and game fixtures only.
