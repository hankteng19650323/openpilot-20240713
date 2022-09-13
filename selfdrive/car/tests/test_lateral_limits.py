#!/usr/bin/env python3
from collections import defaultdict
import importlib
from parameterized import parameterized_class
import unittest

from selfdrive.car.car_helpers import interfaces
from selfdrive.car.fingerprints import all_known_cars
from selfdrive.car.interfaces import get_torque_params


MAX_LAT_UP_JERK = 2.5  # m/s^2
MIN_LAT_DOWN_JERK = 2.0  # m/s^2

jerks = defaultdict(dict)


@parameterized_class('car_model', [(c,) for c in all_known_cars()])
class TestLateralLimits(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    CarInterface, _, _ = interfaces[cls.car_model]
    CP = CarInterface.get_params(cls.car_model)

    if CP.dashcamOnly:
      raise unittest.SkipTest("Platform is behind dashcamOnly")

    # TODO: test these
    if CP.carName in ("honda", "nissan", "body"):
      raise unittest.SkipTest("No steering safety")

    CarControllerParams = importlib.import_module(f'selfdrive.car.{CP.carName}.values').CarControllerParams
    cls.control_params = CarControllerParams(CP)

    cls.torque_params = get_torque_params(cls.car_model)

  @classmethod
  def tearDownClass(cls):
    for car, _jerks in jerks.items():
      print(f'{car:37} - up jerk: {round(_jerks["up_jerk"], 2)} m/s^3, down jerk: {round(_jerks["down_jerk"], 2)} m/s^3')
    print()
    # print(dict(jerks))

  def _calc_jerk(self):
    # TODO: some cars don't send at 100hz, put steer rate/step into CCP to calculate this properly
    steer_step = self.control_params.STEER_STEP
    time_to_max = self.control_params.STEER_MAX / self.control_params.STEER_DELTA_UP / 100. * steer_step
    time_to_min = self.control_params.STEER_MAX / self.control_params.STEER_DELTA_DOWN / 100. * steer_step
    max_lat_accel = self.torque_params['LAT_ACCEL_FACTOR']

    return max_lat_accel / time_to_max, max_lat_accel / time_to_min

  def test_jerk_limits(self):
    up_jerk, down_jerk = self._calc_jerk()
    jerks[self.car_model] = {"up_jerk": up_jerk, "down_jerk": down_jerk}
    self.assertLess(up_jerk, MAX_LAT_UP_JERK)
    # self.assertGreater(down_jerk, MIN_LAT_DOWN_JERK)


if __name__ == "__main__":
  unittest.main()
