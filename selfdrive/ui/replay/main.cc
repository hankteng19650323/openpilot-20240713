#include <termios.h>
#include <iostream>

#include <QApplication>
#include <QThread>

#include "selfdrive/ui/replay/replay.h"

// TODO handle key press here
std::atomic<bool> exit_ = false;

int getch() {
  int ch;
  struct termios oldt;
  struct termios newt;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

  return ch;
}

void keyboardThread(Replay *replay) {
  char c;
  while (!exit_) {
    c = getch();
    if (c == '\n') {
      printf("Enter seek request: ");
      std::string r;
      std::cin >> r;

      try {
        if (r[0] == '#') {
          r.erase(0, 1);
          replay->seekTo(std::stoi(r) * 60);
        } else {
          replay->seekTo(std::stoi(r));
        }
      } catch (std::invalid_argument) {
        qDebug() << "invalid argument";
      }
      getch();  // remove \n from entering seek
    } else if (c == 'm') {
      replay->relativeSeek(+60);
    } else if (c == 'M') {
      replay->relativeSeek(-60);
    } else if (c == 's') {
      replay->relativeSeek(+10);
    } else if (c == 'S') {
      replay->relativeSeek(-10);
    } else if (c == 'G') {
      replay->relativeSeek(0);
    }
  }
}

int main(int argc, char *argv[]){
  QApplication app(argc, argv);

  QString route(argv[1]);
  if (route == "") {
    printf("Usage: ./replay \"route\"\n");
    return 1;
  }
  Replay *replay = new Replay(nullptr, &app);
  replay->load(route);

  QThread *t = QThread::create(keyboardThread, replay);
  QObject::connect(t, &QThread::finished, t, &QThread::deleteLater);
  t->start();

  return app.exec();
}
