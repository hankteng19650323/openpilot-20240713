#pragma once

#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QJsonDocument>
#include <QMainWindow>
#include <QProgressBar>
#include <QSplitter>
#include <QStackedLayout>
#include <QStatusBar>

#include "selfdrive/ui/qt/widgets/controls.h"
#include "tools/cabana/chartswidget.h"
#include "tools/cabana/detailwidget.h"
#include "tools/cabana/messageswidget.h"
#include "tools/cabana/videowidget.h"
#include "tools/cabana/tools/findsimilarbits.h"

class DownloadProgressBar : public QProgressBar {
public:
  DownloadProgressBar(QWidget *parent);
  void updateProgress(uint64_t cur, uint64_t total, bool success);
};

class LoadRouteDialog : public QDialog {
public:
  LoadRouteDialog(const QString &route, const QString &data_dir, uint32_t replay_flags, QWidget *parent);
  void loadRoute(const QString &route, const QString &data_dir, uint32_t replay_flags);
  QString route_string;
  DownloadProgressBar *progress_bar;

protected:
  void loadClicked();
  void reject() override;

  QLineEdit *route_edit;
  QLabel *loading_label;
  QLabel *title_label;
  QStackedLayout *stacked_layout;
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  void dockCharts(bool dock);
  void showStatusMessage(const QString &msg, int timeout = 0) { statusBar()->showMessage(msg, timeout); }

public slots:
  void loadDBCFromName(const QString &name);
  void loadDBCFromFingerprint();
  void loadDBCFromFile();
  void loadDBCFromClipboard();
  void saveDBCToFile();
  void saveDBCToClipboard();
  void loadRoute(const QString &route = {}, const QString &data_dir = {}, uint32_t replay_flags = REPLAY_FLAG_NONE);

signals:
  void showMessage(const QString &msg, int timeout);
  void updateProgressBar(uint64_t cur, uint64_t total, bool success);

protected:
  void createActions();
  void createDockWindows();
  QComboBox *createDBCSelector();
  void createStatusBar();
  void createShortcuts();
  void closeEvent(QCloseEvent *event) override;
  void DBCFileChanged();
  void setOption();
  void findSimilarBits();

  VideoWidget *video_widget;
  QDockWidget *video_dock;
  MessagesWidget *messages_widget;
  DetailWidget *detail_widget;
  ChartsWidget *charts_widget;
  QWidget *floating_window = nullptr;
  QVBoxLayout *r_layout;
  DownloadProgressBar *progress_bar;
  QJsonDocument fingerprint_to_dbc;
  QComboBox *dbc_combo;
};
