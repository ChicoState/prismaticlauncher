#pragma once

#include <QWidget>

class ModBrowser;
class NexusModClient;

class NexusPage : public QWidget {
    Q_OBJECT
   public:
    explicit NexusPage(QWidget* parent = nullptr, bool showBack = true);
    void fetchMods();

   signals:
    void backRequested();

   private:
    NexusModClient* client_;
    ModBrowser* browser_;
    bool loading_ = false;
};
