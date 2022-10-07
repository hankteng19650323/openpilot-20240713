#include "tools/cabana/parser.h"

#include <QDebug>

#include "cereal/messaging/messaging.h"

Parser *parser = nullptr;

static bool event_filter(const Event *e, void *opaque) {
  Parser *p = (Parser *)opaque;
  return p->eventFilter(e);
}

Parser::Parser(QObject *parent) : QObject(parent) {
  parser = this;

  qRegisterMetaType<std::vector<CanData>>();
  QObject::connect(this, &Parser::received, this, &Parser::process, Qt::QueuedConnection);
}

Parser::~Parser() {
  replay->stop();
}

bool Parser::loadRoute(const QString &route, const QString &data_dir, bool use_qcam) {
  replay = new Replay(route, {"can", "roadEncodeIdx"}, {}, nullptr, use_qcam ? REPLAY_FLAG_QCAMERA : 0, data_dir, this);
  replay->installEventFilter(event_filter, this);
  QObject::connect(replay, &Replay::segmentsMerged, this, &Parser::segmentsMerged);
  if (replay->load()) {
    replay->start();
    return true;
  }
  return false;
}

void Parser::openDBC(const QString &name) {
  dbc_name = name;
  dbc = const_cast<DBC *>(dbc_lookup(name.toStdString()));
  counters.clear();
  msg_map.clear();
  for (auto &msg : dbc->msgs) {
    msg_map[msg.address] = &msg;
  }
}

void Parser::process(std::vector<CanData> msgs) {
  static double prev_update_ts = 0;

  for (const auto &can_data : msgs) {
    auto &m = can_msgs[can_data.id];
    while (m.size() >= CAN_MSG_LOG_SIZE) {
      m.pop_front();
    }
    m.push_back(can_data);
  }
  double now = millis_since_boot();
  if ((now - prev_update_ts) > 1000.0 / FPS) {
    prev_update_ts = now;
    emit updated();
  }

  if (current_sec < begin_sec || current_sec > end_sec) {
    // loop replay in selected range.
    seekTo(begin_sec);
  }
}

bool Parser::eventFilter(const Event *event) {
  // drop packets when the GUI thread is calling seekTo. to make sure the current_sec is accurate.
  if (!seeking && event->which == cereal::Event::Which::CAN) {
    current_sec = (event->mono_time - replay->routeStartTime()) / (double)1e9;

    auto can = event->event.getCan();
    msgs_buf.clear();
    msgs_buf.reserve(can.size());

    for (const auto &c : can) {
      CanData &data = msgs_buf.emplace_back();
      data.address = c.getAddress();
      data.bus_time = c.getBusTime();
      data.source = c.getSrc();
      data.dat.append((char *)c.getDat().begin(), c.getDat().size());
      data.id = QString("%1:%2").arg(data.source).arg(data.address, 1, 16);
      data.ts = current_sec;
      data.count = ++counters[data.id];
    }
    emit received(msgs_buf);
  }
  return true;
}

void Parser::seekTo(double ts) {
  seeking = true;
  replay->seekTo(ts, false);
  seeking = false;
}

void Parser::addNewMsg(const Msg &msg) {
  dbc->msgs.push_back(msg);
  msg_map[dbc->msgs.back().address] = &dbc->msgs.back();
}

void Parser::removeSignal(const QString &id, const QString &sig_name) {
  Msg *msg = const_cast<Msg *>(getDBCMsg(id));
  if (!msg) return;

  auto it = std::find_if(msg->sigs.begin(), msg->sigs.end(), [=](auto &sig) { return sig_name == sig.name.c_str(); });
  if (it != msg->sigs.end()) {
    msg->sigs.erase(it);
    emit signalRemoved(id, sig_name);
  }
}

uint32_t Parser::addressFromId(const QString &id) {
  return id.mid(id.indexOf(':') + 1).toUInt(nullptr, 16);
}

const Signal *Parser::getSig(const QString &id, const QString &sig_name) const {
  if (auto msg = getDBCMsg(id)) {
    auto it = std::find_if(msg->sigs.begin(), msg->sigs.end(), [&](auto &s) { return sig_name == s.name.c_str(); });
    if (it != msg->sigs.end()) {
      return &(*it);
    }
  }
  return nullptr;
}

void Parser::setRange(double min, double max) {
  if (begin_sec != min || end_sec != max) {
    begin_sec = min;
    end_sec = max;
    is_zoomed = begin_sec != event_begin_sec || end_sec != event_end_sec;
    emit rangeChanged(min, max);
  }
}

void Parser::segmentsMerged() {
  auto events = replay->events();
  if (!events || events->empty()) return;

  auto it = std::find_if(events->begin(), events->end(), [=](const Event *e) { return e->which == cereal::Event::Which::CAN; });
  event_begin_sec = it == events->end() ? 0 : ((*it)->mono_time - replay->routeStartTime()) / (double)1e9;
  event_end_sec = double(events->back()->mono_time - replay->routeStartTime()) / 1e9;
  if (!is_zoomed) {
    begin_sec = event_begin_sec;
    end_sec = event_end_sec;
  }
  emit eventsMerged();
}

void Parser::resetRange() {
  setRange(event_begin_sec, event_end_sec);
}

// helper functions

static QVector<int> BIG_ENDIAN_START_BITS = []() {
  QVector<int> ret;
  for (int i = 0; i < 64; i++) {
    for (int j = 7; j >= 0; j--) {
      ret.push_back(j + i * 8);
    }
  }
  return ret;
}();

int bigEndianStartBitsIndex(int start_bit) {
  return BIG_ENDIAN_START_BITS[start_bit];
}

int bigEndianBitIndex(int index) {
  return BIG_ENDIAN_START_BITS.indexOf(index);
}

double get_raw_value(uint8_t *data, size_t data_size, const Signal &sig) {
  int64_t val = 0;

  int i = sig.msb / 8;
  int bits = sig.size;
  while (i >= 0 && i < data_size && bits > 0) {
    int lsb = (int)(sig.lsb / 8) == i ? sig.lsb : i * 8;
    int msb = (int)(sig.msb / 8) == i ? sig.msb : (i + 1) * 8 - 1;
    int size = msb - lsb + 1;

    uint64_t d = (data[i] >> (lsb - (i * 8))) & ((1ULL << size) - 1);
    val |= d << (bits - size);

    bits -= size;
    i = sig.is_little_endian ? i - 1 : i + 1;
  }
  if (sig.is_signed) {
    val -= ((val >> (sig.size - 1)) & 0x1) ? (1ULL << sig.size) : 0;
  }
  double value = val * sig.factor + sig.offset;
  return value;
}
