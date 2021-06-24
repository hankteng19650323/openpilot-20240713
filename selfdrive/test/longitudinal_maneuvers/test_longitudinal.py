#!/usr/bin/env python3
import os
import unittest

from common.params import Params
from selfdrive.test.longitudinal_maneuvers.maneuver import Maneuver
from selfdrive.manager.process_config import managed_processes


def check_no_collision(log):
  return min(log['d_rel']) > 0


def check_fcw(log):
  return any(log['fcw'])


def check_engaged(log):
  return log['controls_state_msgs'][-1][-1].active


def put_default_car_params():
  from selfdrive.car.honda.values import CAR
  from selfdrive.car.honda.interface import CarInterface
  cp = CarInterface.get_params(CAR.CIVIC)
  Params().put("CarParams", cp.to_bytes())


maneuvers = [
  Maneuver(
    "fcw: traveling at 30 m/s and approaching lead traveling at 20m/s",
    duration=15.,
    initial_speed=30.,
    lead_relevancy=True,
    initial_distance_lead=100.,
    speed_lead_values=[20.],
    speed_lead_breakpoints=[1.],
    checks=[check_fcw],
  ),
  Maneuver(
    "fcw: traveling at 20 m/s following a lead that decels from 20m/s to 0 at 3m/s2",
    duration=13.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=35.,
    speed_lead_values=[20., 0.],
    speed_lead_breakpoints=[3., 10.],
    checks=[check_fcw],
  ),
  Maneuver(
    "fcw: traveling at 20 m/s following a lead that decels from 20m/s to 0 at 5m/s2",
    duration=8.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=35.,
    speed_lead_values=[20., 0.],
    speed_lead_breakpoints=[3., 7.],
    checks=[check_fcw],
  ),
  Maneuver(
    'steady state following a car at 20m/s, then lead decel to 0mph at 1m/s^2',
    duration=50.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=35.,
    speed_lead_values=[20., 20., 0.],
    speed_lead_breakpoints=[0., 15., 35.0],
    checks=[check_engaged, check_no_collision],
  ),
  Maneuver(
    'steady state following a car at 20m/s, then lead decel to 0mph at 2m/s^2',
    duration=50.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=35.,
    speed_lead_values=[20., 20., 0.],
    speed_lead_breakpoints=[0., 15., 25.0],
    checks=[check_engaged, check_no_collision],
  ),
  Maneuver(
    'steady state following a car at 20m/s, then lead decel to 0mph at 3m/s^2',
    duration=50.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=35.,
    speed_lead_values=[20., 20., 0.],
    speed_lead_breakpoints=[0., 15., 21.66],
    checks=[check_engaged, check_fcw],
  ),
  Maneuver(
    'steady state following a car at 20m/s, then lead decel to 0mph at 5m/s^2',
    duration=40.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=35.,
    speed_lead_values=[20., 20., 0.],
    speed_lead_breakpoints=[0., 15., 19.],
    checks=[check_engaged, check_fcw],
  ),
  Maneuver(
    "approach stopped car at 20m/s",
    duration=30.,
    initial_speed=20.,
    lead_relevancy=True,
    initial_distance_lead=50.,
    speed_lead_values=[0., 0.],
    speed_lead_breakpoints=[1., 11.],
    checks=[check_engaged, check_no_collision],
  ),
]


class LongitudinalControl(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    os.environ['SIMULATION'] = "1"
    os.environ['SKIP_FW_QUERY'] = "1"
    os.environ['NO_CAN_TIMEOUT'] = "1"

    params = Params()
    params.clear_all()
    params.put_bool("Passive", bool(os.getenv("PASSIVE")))
    params.put_bool("OpenpilotEnabledToggle", True)
    params.put_bool("CommunityFeaturesToggle", True)

  # hack
  def test_longitudinal_setup(self):
    pass


def run_maneuver_worker(k):
  def run(self):
    man = maneuvers[k]
    print(man.title)
    put_default_car_params()
    managed_processes['plannerd'].start()

    valid = man.evaluate()

    managed_processes['plannerd'].stop()
    self.assertTrue(valid)
  return run


for k in range(len(maneuvers)):
  setattr(LongitudinalControl, f"test_longitudinal_maneuvers_{k+1}",
          run_maneuver_worker(k))

if __name__ == "__main__":
  unittest.main(failfast=True)
