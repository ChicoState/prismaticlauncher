# ChickPix settings

## Objective and acceptance criteria

Let launcher users choose bundled or custom ChickPix artwork and adjust its
visibility without leaving launcher settings.

- Place **Configure ChickPix** immediately below **Save game path**, with the
  same button styling as that button and **Auto Detect Directory**.
- Offer bundled Default, Spring, and Summer images, and readable images from `chickens` beside the
  settings file or launcher executable. Do not depend on the working directory.
- Show an image preview, enabled switch, opacity percentage (0–100), folder
  locations, and a refresh action for newly added files.
- Save image, enabled state, and opacity together in the existing launcher INI.
  Cancel leaves saved values unchanged. Report write failures visibly.
- Apply saved changes immediately to the library artwork and the **ChickPix** toggle;
  reload saved settings when the configuration dialog opens.
- On first show, display the saved image and opacity when enabled, and keep it
  hidden when disabled. Lay out artwork within the library's final bounds without
  requiring a toggle, resize, or settings-file write.
- Keep opacity exclusively in the configuration dialog. Remove seasonal selection
  and use Default artwork for empty legacy settings or unavailable images.
- Show only filenames in the image dropdown. Collapse duplicate decoded images
  and repeated filenames (ignoring case), preferring bundled images, then the
  settings folder, then the executable folder. Resolve saved duplicates to their
  retained entry in both the library and preview without adding a duplicate row.
  Show full paths in tooltips.
- Retain an unavailable saved filename and explain the Default fallback in settings.
- Ignore unsupported, corrupt, oversized images and symlinks escaping the custom
  folders. Treat filenames as plain text.

## Implementation and validation plan

1. Add focused Qt widget tests in `tests/ui/ChickPixTests.cpp`, registered with
   CTest, and observe failure before changing application behavior.
2. Implement validated image discovery/loading and settings persistence in
   `src/qt/`; add the dialog and settings entry, then integrate library refresh.
3. Run focused tests, the complete CTest suite, relevant format/static checks,
   and infrastructure validation. Independently review the final diff.

Build with `cmake --build --preset dev -j 4`; run focused tests with
`ctest --preset dev -R chickpix --output-on-failure` and the complete suite with
`ctest --preset dev --output-on-failure`. Infrastructure checks use
`./scripts/verify-infrastructure.sh`, `./scripts/format-check.sh`, and
`./scripts/static-analysis.sh` when prerequisites are installed.

Use C++20/Qt 6 and the existing stylesheet with no new dependencies. Follow
`.clang-format`, descriptive names, explicit control flow, and one authoritative
`/// @brief` summary per new or substantially changed function, for example:

```cpp
/// @brief Reloads saved artwork settings and refreshes the library overlay.
void reloadChickPix();
```

Keep changes within ChickPix application code, tests, and user documentation.
Preserve local Qt Creator files. Do not stage, commit, publish, change
infrastructure decisions, or expand into unrelated launcher features.

## Startup geometry

Watch the library page's Resize and Show events when positioning the artwork.
The main window can receive its resize event before the stacked library page has
its final size; positioning at that point left enabled artwork outside the page.
Qt supplies the resized widget's updated geometry with its
[resize event](https://doc.qt.io/qt-6/qwidget.html#resizeEvent). The
[event filter](https://doc.qt.io/qt-6/qobject.html#eventFilter) leaves normal event
handling intact. Startup tests inspect actual rendered pixels, containment,
saved opacity, and both enabled and disabled states without a corrective toggle.
