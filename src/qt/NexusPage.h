#pragma once

#include <QWidget>

class QNetworkAccessManager;
class ModBrowser;
class NexusModClient;

class NexusPage : public QWidget {
    Q_OBJECT
   public:
    explicit NexusPage(QWidget* parent = nullptr, bool showBack = true,
                       QNetworkAccessManager* networkManager = nullptr);
    void fetchMods();

   signals:
    void backRequested();

   private:
    void requestPage(int offset);
    void updatePagination();

    int offset_ = 0;
    int requestedOffset_ = 0;
    qint64 totalCount_ = 0;
    int returnedCount_ = 0;
    bool hasPage_ = false;
    NexusModClient* client_;
    ModBrowser* browser_;
    bool loading_ = false;
};
