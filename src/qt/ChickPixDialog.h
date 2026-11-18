#pragma once

#include <QDialog>
#include <QPixmap>

#include "ChickPixImages.h"

class LauncherConfig;
class QCheckBox;
class QComboBox;
class QGraphicsOpacityEffect;
class QLabel;
class QSlider;

class ChickPixDialog : public QDialog {
    Q_OBJECT

   public:
    /// @brief Loads saved ChickPix values into a modal editor with an isolated preview.
    explicit ChickPixDialog(LauncherConfig* config, QWidget* parent = nullptr);
    /// @brief Discards pending edits and reloads the saved settings and image catalog.
    void reload();

   signals:
    /// @brief Announces that all three pending values were successfully synced to the INI.
    void saved();

   private:
    /// @brief Lists unique image filenames and resolves the pending choice to its retained entry.
    void refreshCatalog();
    /// @brief Decodes the pending image or Default fallback and refreshes its preview.
    void loadPreview();
    /// @brief Applies pending visibility and opacity to the cached preview.
    void updatePreview();
    /// @brief Saves pending values and reports failures without announcing success.
    void save();

    LauncherConfig* config_;
    ChickPixImages images_;
    QComboBox* image_;
    QSlider* opacity_;
    QCheckBox* enabled_;
    QLabel* opacityValue_;
    QLabel* status_;
    QLabel* preview_;
    QGraphicsOpacityEffect* previewOpacity_;
    QPixmap previewSource_;
};
