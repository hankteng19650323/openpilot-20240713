#include "tools/cabana/chartswidget.h"

#include <QGraphicsLayout>
#include <QLabel>
#include <QPushButton>
#include <QtCharts/QLineSeries>

int64_t get_raw_value(const QByteArray &msg, const Signal &sig) {
  int64_t ret = 0;

  int i = sig.msb / 8;
  int bits = sig.size;
  while (i >= 0 && i < msg.size() && bits > 0) {
    int lsb = (int)(sig.lsb / 8) == i ? sig.lsb : i * 8;
    int msb = (int)(sig.msb / 8) == i ? sig.msb : (i + 1) * 8 - 1;
    int size = msb - lsb + 1;

    uint64_t d = (msg[i] >> (lsb - (i * 8))) & ((1ULL << size) - 1);
    ret |= d << (bits - size);

    bits -= size;
    i = sig.is_little_endian ? i - 1 : i + 1;
  }
  return ret;
}

ChartsWidget::ChartsWidget(QWidget *parent) : QWidget(parent) {
  main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  connect(parser, &Parser::showPlot, this, &ChartsWidget::addChart);
  connect(parser, &Parser::hidePlot, this, &ChartsWidget::removeChart);
}

void ChartsWidget::addChart(const QString &id, const QString &sig_name) {
  const QString char_name = id + sig_name;
  if (charts.find(char_name) == charts.end()) {
    auto chart = new ChartWidget(id, sig_name, this);
    QObject::connect(chart, &ChartWidget::remove, this, &ChartsWidget::removeChart);
    main_layout->addWidget(chart);
    charts[char_name] = chart;
  }
}

void ChartsWidget::removeChart(const QString &id, const QString &sig_name) {
  if (auto it = charts.find(id + sig_name); it != charts.end()) {
    delete it->second;
    charts.erase(it);
  }
}

ChartWidget::ChartWidget(const QString &id, const QString &sig_name, QWidget *parent) : id(id), sig_name(sig_name), QWidget(parent) {
  // TODO: drag to select the range of axisX
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setSpacing(0);
  main_layout->setContentsMargins(0, 0, 0, 0);

  QWidget *header = new QWidget(this);
  header->setStyleSheet("background-color:white");
  QHBoxLayout *header_layout = new QHBoxLayout(header);
  header_layout->setContentsMargins(11, 11, 11, 0);
  auto title = new QLabel(tr("%1 %2").arg(parser->getMsg(id)->name.c_str()).arg(id));
  header_layout->addWidget(title);
  header_layout->addStretch();

  QPushButton *remove_btn = new QPushButton(tr("Hide plot"), this);
  QObject::connect(remove_btn, &QPushButton::clicked, [=]() {
    emit remove(id, sig_name);
  });
  header_layout->addWidget(remove_btn);
  main_layout->addWidget(header);

  QLineSeries *series = new QLineSeries();
  series->setUseOpenGL(true);
  auto chart = new QChart();
  chart->setTitle(sig_name);
  chart->addSeries(series);
  chart->createDefaultAxes();
  chart->legend()->hide();
  QFont font;
  font.setBold(true);
  chart->setTitleFont(font);
  chart->setMargins({0, 0, 0, 0});
  chart->layout()->setContentsMargins(0, 0, 0, 0);

  chart_view = new QChartView(chart);
  chart_view->setFixedHeight(300);
  chart_view->setRenderHint(QPainter::Antialiasing);
  main_layout->addWidget(chart_view);
  main_layout->addStretch();

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(parser, &Parser::updated, this, &ChartWidget::updateState);
}

void ChartWidget::updateState() {
  static double last_update = millis_since_boot();
  double current_ts = millis_since_boot();
  bool update = (current_ts - last_update) > 500;
  if (update) {
    last_update = current_ts;
  }

  auto getSig = [=]() -> const Signal * {
    for (auto &sig : parser->getMsg(id)->sigs) {
      if (sig_name == sig.name.c_str()) return &sig;
    }
    return nullptr;
  };

  const Signal *sig = getSig();
  if (!sig) return;

  const auto &can_data = parser->can_msgs[id].back();
  int64_t val = get_raw_value(can_data.dat, *sig);
  if (sig->is_signed) {
    val -= ((val >> (sig->size - 1)) & 0x1) ? (1ULL << sig->size) : 0;
  }
  double value = val * sig->factor + sig->offset;

  if (value > max_y) max_y = value;
  if (value < min_y) min_y = value;

  while (data.size() > DATA_LIST_SIZE) {
    data.pop_front();
  }
  data.push_back({can_data.ts / 1000., value});

  if (update) {
    QChart *chart = chart_view->chart();
    QLineSeries *series = (QLineSeries *)chart->series()[0];
    series->replace(data);
    chart->axisX()->setRange(data.front().x(), data.back().x());
    chart->axisY()->setRange(min_y, max_y);
  }
}
