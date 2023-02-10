#include "tools/cabana/dbcmanager.h"

#include <QFile>
#include <QRegExp>
#include <QTextStream>
#include <QVector>
#include <limits>
#include <sstream>

namespace dbcmanager {

void sortSignalsByAddress(QList<Signal> &sigs) {
  std::sort(sigs.begin(), sigs.end(), [](auto &a, auto &b) { return a.start_bit < b.start_bit; });
}

bool DBCManager::open(const QString &dbc_file_name, QString *error) {
  QString opendbc_file_path = QString("%1/%2.dbc").arg(OPENDBC_FILE_PATH, dbc_file_name);
  QFile file(opendbc_file_path);
  if (file.open(QIODevice::ReadOnly)) {
    return open(dbc_file_name, file.readAll(), error);
  }
  return false;
}

void DBCManager::parseExtraInfo(const QString &content) {
  static QRegExp bo_regexp(R"(^BO_ (\w+) (\w+) *: (\w+) (\w+))");
  static QRegExp sg_regexp(R"(^SG_ (\w+) : (\d+)\|(\d+)@(\d+)([\+|\-]) \(([0-9.+\-eE]+),([0-9.+\-eE]+)\) \[([0-9.+\-eE]+)\|([0-9.+\-eE]+)\] \"(.*)\" (.*))");
  static QRegExp sgm_regexp(R"(^SG_ (\w+) (\w+) *: (\d+)\|(\d+)@(\d+)([\+|\-]) \(([0-9.+\-eE]+),([0-9.+\-eE]+)\) \[([0-9.+\-eE]+)\|([0-9.+\-eE]+)\] \"(.*)\" (.*))");
  static QRegExp sg_comment_regexp(R"(^CM_ SG_ *(\w+) *(\w+) *\"(.*)\";)");
  static QRegExp val_regexp(R"(VAL_ (\w+) (\w+) (.*);)");
  auto get_sig = [this](uint32_t address, const QString &name) -> Signal * {
    auto m = (Msg *)msg(address);
    return m ? (Signal *)m->sig(name) : nullptr;
  };

  QTextStream stream((QString *)&content);
  uint32_t address = 0;
  while (!stream.atEnd()) {
    QString line = stream.readLine().trimmed();
    if (line.startsWith("BO_ ") && bo_regexp.indexIn(line) != -1) {
      address = bo_regexp.capturedTexts()[1].toUInt();
    } else if (line.startsWith("SG_ ")) {
      QStringList result;
      if (sg_regexp.indexIn(line) != -1) {
        result = sg_regexp.capturedTexts();
      } else if (sgm_regexp.indexIn(line) != -1) {
        result = sgm_regexp.capturedTexts();
        result.removeAt(0);
      }
      if (!result.isEmpty()) {
        if (auto s = get_sig(address, result[1])) {
          s->min = result[8];
          s->max = result[9];
          s->unit = result[10];
        }
      }
    } else if (line.startsWith("VAL_ ") && val_regexp.indexIn(line) != -1) {
      auto result = val_regexp.capturedTexts();
      if (auto s = get_sig(result[1].toUInt(), result[2])) {
        s->val_desc = result[3].trimmed();
      }
    } else if (line.startsWith("CM_ SG_ ") && sg_comment_regexp.indexIn(line) != -1) {
      auto result = sg_comment_regexp.capturedTexts();
      if (auto s = get_sig(result[1].toUInt(), result[2])) {
        s->comment = result[3];
      }
    }
  }
}

QString DBCManager::generateDBC() {
  QString dbc_string, signal_comment, val_desc;
  for (auto &[address, m] : msgs) {
    dbc_string += QString("BO_ %1 %2: %3 XXX\n").arg(address).arg(m.name).arg(m.size);
    for (auto &sig : m.sigs) {
      dbc_string += QString(" SG_ %1 : %2|%3@%4%5 (%6,%7) [%8|%9] \"%10\" XXX\n")
                        .arg(sig.name)
                        .arg(sig.start_bit)
                        .arg(sig.size)
                        .arg(sig.is_little_endian ? '1' : '0')
                        .arg(sig.is_signed ? '-' : '+')
                        .arg(sig.factor, 0, 'g', std::numeric_limits<double>::digits10)
                        .arg(sig.offset, 0, 'g', std::numeric_limits<double>::digits10)
                        .arg(sig.min)
                        .arg(sig.max)
                        .arg(sig.unit);
      if (!sig.comment.isEmpty()) {
        signal_comment += QString("CM_ SG_ %1 %2 \"%3\";\n").arg(address).arg(sig.name).arg(sig.comment);
      }
      if (!sig.val_desc.isEmpty()) {
        val_desc += QString("VAL_ %1 %2 %3;\n").arg(address).arg(sig.name).arg(sig.val_desc);
      }
    }
    dbc_string += "\n";
  }
  return dbc_string + signal_comment + val_desc;
}

void DBCManager::updateMsg(const QString &id, const QString &name, uint32_t size) {
  auto [_, address] = parseId(id);
  auto &m = msgs[address];
  m.name = name;
  m.size = size;
  emit msgUpdated(address);
}

void DBCManager::removeMsg(const QString &id) {
  uint32_t address = parseId(id).second;
  msgs.erase(address);
  emit msgRemoved(address);
}

void DBCManager::addSignal(const QString &id, const Signal &sig) {
  if (auto m = const_cast<Msg *>(msg(id))) {
    m->sigs.push_back(sig);
    auto s = &m->sigs.last();
    sortSignalsByAddress(m->sigs);
    emit signalAdded(parseId(id).second, s);
  }
}

void DBCManager::updateSignal(const QString &id, const QString &sig_name, const Signal &sig) {
  if (auto m = const_cast<Msg *>(msg(id))) {
    if (auto s = (Signal *)m->sig(sig_name)) {
      *s = sig;
      sortSignalsByAddress(m->sigs);
      emit signalUpdated(s);
    }
  }
}

void DBCManager::removeSignal(const QString &id, const QString &sig_name) {
  if (auto m = const_cast<Msg *>(msg(id))) {
    auto it = std::find_if(m->sigs.begin(), m->sigs.end(), [&](auto &s) { return s.name == sig_name; });
    if (it != m->sigs.end()) {
      emit signalRemoved(&(*it));
      m->sigs.erase(it);
    }
  }
}

std::pair<uint8_t, uint32_t> DBCManager::parseId(const QString &id) {
  const auto list = id.split(':');
  if (list.size() != 2) return {0, 0};
  return {list[0].toInt(), list[1].toUInt(nullptr, 16)};
}

DBCManager *dbc() {
  static DBCManager dbc_manager(nullptr);
  return &dbc_manager;
}

// helper functions

static QVector<int> BIG_ENDIAN_START_BITS = []() {
  QVector<int> ret;
  for (int i = 0; i < 64; i++)
    for (int j = 7; j >= 0; j--)
      ret.push_back(j + i * 8);
  return ret;
}();

int bigEndianStartBitsIndex(int start_bit) { return BIG_ENDIAN_START_BITS[start_bit]; }
int bigEndianBitIndex(int index) { return BIG_ENDIAN_START_BITS.indexOf(index); }

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
  return val * sig.factor + sig.offset;
}

void updateSigSizeParamsFromRange(Signal &s, int start_bit, int size) {
  s.start_bit = s.is_little_endian ? start_bit : bigEndianBitIndex(start_bit);
  s.size = size;
  if (s.is_little_endian) {
    s.lsb = s.start_bit;
    s.msb = s.start_bit + s.size - 1;
  } else {
    s.lsb = bigEndianStartBitsIndex(bigEndianBitIndex(s.start_bit) + s.size - 1);
    s.msb = s.start_bit;
  }
}

std::pair<int, int> getSignalRange(const Signal *s) {
  int from = s->is_little_endian ? s->start_bit : bigEndianBitIndex(s->start_bit);
  int to = from + s->size - 1;
  return {from, to};
}

bool operator==(const Signal &l, const Signal &r) {
  return l.name == r.name && l.size == r.size &&
         l.start_bit == r.start_bit &&
         l.msb == r.msb && l.lsb == r.lsb &&
         l.is_signed == r.is_signed && l.is_little_endian == r.is_little_endian &&
         l.factor == r.factor && l.offset == r.offset &&
         l.min == r.min && l.max == r.max && l.comment == r.comment && l.unit == r.unit && l.val_desc == r.val_desc;
}

}  // namespace dbcmanager

#include "opendbc/can/common_dbc.h"
std::vector<std::string> dbcmanager::DBCManager::allDBCNames() { return get_dbc_names(); }

bool dbcmanager::DBCManager::open(const QString &name, const QString &content, QString *error) {
  try {
    std::istringstream stream(content.toStdString());
    auto dbc = const_cast<DBC *>(dbc_parse_from_stream(name.toStdString(), stream));
    msgs.clear();
    for (auto &msg : dbc->msgs) {
      auto &m = msgs[msg.address];
      m.name = msg.name.c_str();
      m.size = msg.size;
      for (auto &s : msg.sigs) {
        m.sigs.push_back({});
        auto &sig = m.sigs.last();
        sig.name = s.name.c_str();
        sig.start_bit = s.start_bit;
        sig.msb = s.msb;
        sig.lsb = s.lsb;
        sig.size = s.size;
        sig.is_signed = s.is_signed;
        sig.factor = s.factor;
        sig.offset = s.offset;
        sig.is_little_endian = s.is_little_endian;
      }
      sortSignalsByAddress(m.sigs);
    }
    parseExtraInfo(content);
    name_ = name;
    emit DBCFileChanged();
    delete dbc;
  } catch (std::exception &e) {
    if (error) *error = e.what();
    return false;
  }
  return true;
}
