#pragma once

#include <QWidget>

class NexusModClient;
class QLabel;
class QListWidget;
class QPushButton;

class NexusPage : public QWidget {
    Q_OBJECT
   public:
    explicit NexusPage(QWidget* parent = nullptr);
    void fetchMods();

   signals:
    void backRequested();

   private:
    NexusModClient* client_;
    QLabel* status_;
    QListWidget* mods_;
    QPushButton* refresh_;
    bool loading_ = false;
};
