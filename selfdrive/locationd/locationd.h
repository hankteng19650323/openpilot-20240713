#pragma once

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <memory>

#include <eigen3/Eigen/Dense>

#include "messaging.hpp"
#include "common/params.h"
#include "common/util.h"
#include "common/swaglog.h"
#include "common/timing.h"
#include "common/transformations/coordinates.hpp"
#include "common/transformations/orientation.hpp"

#include "models/live_kf.h"

#define DEG2RAD(x) ((x) * M_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / M_PI)

#define VISION_DECIMATION 2
#define SENSOR_DECIMATION 10

#define POSENET_STD_HIST_HALF 20

Eigen::VectorXd floatlist_to_vector(const capnp::List<float, capnp::Kind::PRIMITIVE>::Reader& floatlist);
Eigen::Vector4d quat2vector(const Eigen::Quaterniond& quat);
Eigen::Quaterniond vector2quat(const Eigen::VectorXd& vec);
void initMeasurement(cereal::LiveLocationKalman::Measurement::Builder& meas, const Eigen::VectorXd& val, const Eigen::VectorXd& std, bool valid);

class Localizer {
public:
  Localizer();

  int locationd_thread();

  void reset_kalman(double current_time = NAN);
  void reset_kalman(double current_time, Eigen::VectorXd init_orient, Eigen::VectorXd init_pos);

  kj::ArrayPtr<capnp::byte> getMessageBytes(MessageBuilder& msg_builder, uint64_t logMonoTime,
    bool inputsOK, bool sensorsOK, bool gpsOK);
  void buildLiveLocation(cereal::LiveLocationKalman::Builder& fix);

  Eigen::VectorXd getPositionGeodetic();

  void handle_msg_bytes(const char *data, const size_t size);
  void handle_msg(const cereal::Event::Reader& log);
  void handle_sensors(double current_time, const capnp::List<cereal::SensorEventData, capnp::Kind::STRUCT>::Reader& log);
  void handle_gps(double current_time, const cereal::GpsLocationData::Reader& log);
  void handle_car_state(double current_time, const cereal::CarState::Reader& log);
  void handle_cam_odo(double current_time, const cereal::CameraOdometry::Reader& log);
  void handle_live_calib(double current_time, const cereal::LiveCalibrationData::Reader& log);

private:
  std::shared_ptr<LiveKalman> kf;

  Eigen::VectorXd calib;
  MatrixXdr device_from_calib;
  MatrixXdr calib_from_device;
  bool calibrated = false;

  int car_speed = 0;
  Eigen::VectorXd posenet_stds_old;
  Eigen::VectorXd posenet_stds_new;
  int posenet_stds_i = 0;

  std::shared_ptr<LocalCoord> converter;

  int64_t unix_timestamp_millis = 0;
  double last_gps_fix = 0;
  bool device_fell = false;

  int gyro_counter = 0;
  int acc_counter = 0;
  int speed_counter = 0;
  int cam_counter = 0;
};
