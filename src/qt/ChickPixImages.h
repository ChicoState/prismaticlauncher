#pragma once

#include <QImage>
#include <QStringList>

class ChickPixImages {
   public:
    /// @brief Initializes bundled resources and custom roots beside the config and executable.
    explicit ChickPixImages(const QString& configFile);
    /// @brief Returns the absolute custom folders, independent of the working directory.
    [[nodiscard]] QStringList folders() const;
    /// @brief Returns valid images with unique pixels and filenames, preferring bundled entries.
    [[nodiscard]] QStringList catalog() const;
    /// @brief Maps legacy empty values and duplicate choices to catalog entries; keeps unmatched
    /// paths.
    [[nodiscard]] QString resolveSelection(const QString& selection,
                                           const QStringList& catalog) const;
    /// @brief Decodes an allowed image; returns a null image for invalid or unavailable selections.
    [[nodiscard]] QImage load(const QString& selection) const;
    /// @brief Copies a readable image into the settings-side custom folder without overwriting
    /// existing artwork.
    /// @return The imported absolute path, or an empty string when validation or copying fails.
    [[nodiscard]] QString importImage(const QString& source) const;
    /// @brief Returns the bundled Default artwork identifier used for new or unavailable
    /// selections.
    [[nodiscard]] static QString defaultImage();

   private:
    /// @brief Reuses immutable bundled fingerprints and freshly validates custom image pixels.
    [[nodiscard]] QByteArray fingerprint(const QString& selection) const;
    /// @brief Resolves a bundled identifier or custom path confined to its allowed folder.
    [[nodiscard]] QString allowedPath(const QString& selection) const;
    QStringList folders_;
};
