#pragma once

#include <QWidget>

#include "Nexus_fetchList.hpp"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;

// Presentation only: requests are connected by the owning launcher window.
class ModBrowser : public QWidget {
    Q_OBJECT
   public:
    explicit ModBrowser(QWidget* parent = nullptr);
    void beginLoading();
    void beginPageLoading();
    void setPageError(const QString& message);
    void setPagination(int offset, qint64 total, int count, bool loading);
    void setMods(const QVector<NexusModSummary>& mods);
    void setListError(const QString& message);
    void setModDetails(const NexusModDetails& mod);
    void setDetailsError(const QString& message);

   signals:
    void refreshRequested();
    void previousPageRequested();
    void nextPageRequested();
    // Zero means selection was cleared; the owner should cancel outstanding details.
    void modSelected(qint64 id);

   private:
    void filterMods();
    void selectMod();
    void clearDetails();
    void showSummary(const NexusModSummary& mod);

    QVector<NexusModSummary> mods_;
    qint64 selectedId_ = 0;
    QListWidget* list_;
    QLineEdit* search_;
    QLabel* count_;
    QLabel* status_;
    QLabel* title_;
    QLabel* author_;
    QLabel* metadata_;
    QLabel* summary_;
    QLabel* description_;
    QLabel* detailStatus_;
    QScrollArea* details_;
    QLabel* pageCount_;
    QPushButton* previous_;
    QPushButton* next_;
    QPushButton* refresh_;
    QPushButton* download_;
};
