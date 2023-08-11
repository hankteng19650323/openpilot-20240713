#pragma once

#include <QHash>
#include <QHBoxLayout>
#include <QLabel>

#include "cereal/gen/cpp/log.capnp.h"

class MapInstructions : public QWidget {
  Q_OBJECT

private:
  QLabel *distance;
  QLabel *primary;
  QLabel *secondary;
  QLabel *icon_01;
  QHBoxLayout *lane_layout;
  bool is_rhd = false;
  std::vector<QLabel *> lane_labels;
  QHash<QString, QPixmap> pixmap_cache;

public:
  MapInstructions(QWidget * parent=nullptr);
  void buildPixmapCache();
  QString getDistance(float d);
  void updateInstructions(cereal::NavInstruction::Reader instruction);
};

QString getLaneDirectionName(const std::vector<cereal::NavInstruction::Direction>& directions,
                             bool active, cereal::NavInstruction::Direction active_direction);

static std::map<cereal::NavInstruction::Direction, QString> DIRECTIONS = {
  {cereal::NavInstruction::Direction::NONE, "none"},
  {cereal::NavInstruction::Direction::LEFT, "left"},
  {cereal::NavInstruction::Direction::SLIGHT_LEFT, "slight_left"},
  {cereal::NavInstruction::Direction::STRAIGHT, "straight"},
  {cereal::NavInstruction::Direction::SLIGHT_RIGHT, "slight_right"},
  {cereal::NavInstruction::Direction::RIGHT, "right"},
};
