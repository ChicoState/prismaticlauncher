#include "ChickPixDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "LauncherConfig.h"

ChickPixDialog::ChickPixDialog(LauncherConfig* config, QWidget* parent)
    : QDialog(parent), config_(config), images_(config->filePath()) {
    setObjectName("chickPixDialog");
    setWindowTitle("Configure ChickPix");
    setWindowModality(Qt::ApplicationModal);
    resize(560, 560);
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    auto* form = new QFormLayout;
    image_ = new QComboBox(this);
    image_->setObjectName("chickPixImage");
    image_->setAccessibleName("ChickPix image");
    image_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    image_->setMinimumContentsLength(28);
    form->addRow("&Image", image_);
    enabled_ = new QCheckBox("Show ChickPix artwork", this);
    enabled_->setObjectName("chickPixEnabled");
    enabled_->setChecked(config_->farmPackEnabled());
    form->addRow(enabled_);
    opacity_ = new QSlider(Qt::Horizontal, this);
    opacity_->setObjectName("chickPixOpacity");
    opacity_->setRange(0, 100);
    opacity_->setValue(config_->farmPackOpacityPercent());
    opacity_->setAccessibleName("ChickPix opacity percentage");
    auto* opacityRow = new QHBoxLayout;
    opacityRow->addWidget(opacity_);
    opacityValue_ = new QLabel(this);
    opacityRow->addWidget(opacityValue_);
    auto* opacityLabel = new QLabel("&Opacity", this);
    opacityLabel->setBuddy(opacity_);
    form->addRow(opacityLabel, opacityRow);
    layout->addLayout(form);

    preview_ = new QLabel(this);
    preview_->setObjectName("chickPixPreview");
    preview_->setAccessibleName("Pending ChickPix preview");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(260, 180);
    previewOpacity_ = new QGraphicsOpacityEffect(preview_);
    preview_->setGraphicsEffect(previewOpacity_);
    layout->addWidget(preview_);

    auto* folders = new QLabel("Custom image folders:\n" + images_.folders().join('\n'), this);
    folders->setObjectName("chickPixFolders");
    folders->setTextFormat(Qt::PlainText);
    folders->setWordWrap(true);
    folders->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(folders);
    auto* refresh = new QPushButton("Refresh images", this);
    refresh->setObjectName("chickPixRefresh");
    layout->addWidget(refresh);
    status_ = new QLabel(this);
    status_->setObjectName("chickPixStatus");
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* saveButton = buttons->addButton("Save", QDialogButtonBox::ActionRole);
    saveButton->setObjectName("chickPixSave");
    layout->addWidget(buttons);

    reload();
    connect(image_, &QComboBox::currentIndexChanged, this, &ChickPixDialog::loadPreview);
    connect(opacity_, &QSlider::valueChanged, this, &ChickPixDialog::updatePreview);
    connect(enabled_, &QCheckBox::toggled, this, &ChickPixDialog::updatePreview);
    connect(refresh, &QPushButton::clicked, this, &ChickPixDialog::refreshCatalog);
    connect(saveButton, &QPushButton::clicked, this, &ChickPixDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ChickPixDialog::reload() {
    const QSignalBlocker imageBlocker(image_);
    const QSignalBlocker opacityBlocker(opacity_);
    const QSignalBlocker enabledBlocker(enabled_);
    image_->clear();
    image_->addItem(config_->farmPackImage(), config_->farmPackImage());
    opacity_->setValue(config_->farmPackOpacityPercent());
    enabled_->setChecked(config_->farmPackEnabled());
    refreshCatalog();
}

void ChickPixDialog::refreshCatalog() {
    const auto catalog = images_.catalog();
    const auto selection = images_.resolveSelection(image_->currentData().toString(), catalog);
    const QSignalBlocker blocker(image_);
    image_->clear();
    for (const auto& path : catalog) {
        image_->addItem(QFileInfo(path).fileName(), path);
        image_->setItemData(image_->count() - 1, path, Qt::ToolTipRole);
    }
    if (image_->findData(selection) < 0) {
        image_->addItem(QFileInfo(selection).fileName(), selection);
        image_->setItemData(image_->count() - 1, selection, Qt::ToolTipRole);
    }
    image_->setCurrentIndex(image_->findData(selection));
    loadPreview();
}

void ChickPixDialog::loadPreview() {
    const auto selection = image_->currentData().toString();
    QImage image;
    if (!selection.isEmpty()) image = images_.load(selection);
    if (!selection.isEmpty() && image.isNull()) {
        status_->setText(
            "Selected image is unavailable or invalid. Using Default artwork. "
            "The saved selection is retained until you save another image.");
    } else {
        status_->setText("Preview only. Select Save to apply changes to the library.");
    }
    if (image.isNull()) image = images_.load(ChickPixImages::defaultImage());
    previewSource_ = QPixmap::fromImage(image);
    updatePreview();
}

void ChickPixDialog::updatePreview() {
    opacityValue_->setText(QString::number(opacity_->value()) + "%");
    previewOpacity_->setOpacity(opacity_->value() / 100.0);
    if (!enabled_->isChecked()) {
        preview_->clear();
        return;
    }
    preview_->setPixmap(
        previewSource_.scaled(260, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ChickPixDialog::save() {
    if (!config_->saveChickPix(image_->currentData().toString(), opacity_->value(),
                               enabled_->isChecked())) {
        status_->setText(
            "Could not save ChickPix settings. Check that the settings location is writable.");
        return;
    }
    loadPreview();
    const auto selection = image_->currentData().toString();
    if (selection.isEmpty() || !images_.load(selection).isNull()) {
        status_->setText("Saved ChickPix settings.");
    } else {
        status_->setText("Saved ChickPix settings. " + status_->text());
    }
    emit saved();
}
