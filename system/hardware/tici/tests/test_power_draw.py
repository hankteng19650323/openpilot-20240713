#!/usr/bin/env python3
from collections import defaultdict, deque
import sys
import pytest
import unittest
import time
import numpy as np
from dataclasses import dataclass
from tabulate import tabulate
from typing import List

import cereal.messaging as messaging
from cereal.services import SERVICE_LIST
from openpilot.common.mock import mock_messages
from openpilot.selfdrive.car.car_helpers import write_car_param
from openpilot.system.hardware.tici.power_monitor import get_power
from openpilot.selfdrive.manager.process_config import managed_processes
from openpilot.selfdrive.manager.manager import manager_cleanup

SAMPLE_TIME = 8       # seconds to sample power
MAX_WARMUP_TIME = 30  # seconds to wait for SAMPLE_TIME consecutive valid samples

@dataclass
class Proc:
  name: str
  power: float
  msgs: List[str]
  rtol: float = 0.05
  atol: float = 0.12

PROCS = [
  Proc('camerad', 2.1, msgs=['roadCameraState', 'wideRoadCameraState', 'driverCameraState']),
  Proc('modeld', 1.12, atol=0.2, msgs=['modelV2']),
  Proc('dmonitoringmodeld', 0.4, msgs=['driverStateV2']),
  Proc('encoderd', 0.23, msgs=[]),
  Proc('mapsd', 0.05, msgs=['mapRenderState']),
  Proc('navmodeld', 0.05, msgs=['navModel']),
]


@pytest.mark.tici
class TestPowerDraw(unittest.TestCase):

  def setUp(self):
    write_car_param()

    # wait a bit for power save to disable
    time.sleep(5)

  def tearDown(self):
    manager_cleanup()

  def get_expected_messages(self, proc):
    return int(sum(SAMPLE_TIME * SERVICE_LIST[msg].frequency for msg in proc.msgs))

  def get_power_with_warmup_for_target(self, proc, prev):
    socks = {msg: messaging.sub_sock(msg) for msg in proc.msgs}
    for sock in socks.values():
      messaging.drain_sock_raw(sock)

    msgs_and_power = deque([None] * SAMPLE_TIME, maxlen=SAMPLE_TIME)

    start_time = time.time()

    while (time.time() - start_time) < MAX_WARMUP_TIME:
      power = get_power(1)
      local_msg_counts = {}
      for msg,sock in socks.items():
        local_msg_counts[msg] = len(messaging.drain_sock_raw(sock))
      msgs_and_power.append((power, local_msg_counts))

      if any(m is None for m in msgs_and_power):
        continue

      local_msg_counts2 = defaultdict(lambda: 0)
      for m in msgs_and_power:
        power, z = m
        for msg, count in z.items():
          local_msg_counts2[msg] += count

      msgs_received = sum(local_msg_counts2[msg] for msg in proc.msgs)
      msgs_expected = self.get_expected_messages(proc)

      now = np.mean([m[0] for m in msgs_and_power])
      valid_msg_count = np.core.numeric.isclose(msgs_expected, msgs_received, rtol=.02, atol=2)
      valid_power_draw = np.core.numeric.isclose(now - prev, proc.power, rtol=proc.rtol, atol=proc.atol)

      if valid_msg_count and valid_power_draw:
        break

    msg_counts = defaultdict(lambda: 0)
    for m in msgs_and_power:
      power, z = m
      for msg, count in z.items():
        msg_counts[msg] += count

    return now, msg_counts, time.time() - start_time - SAMPLE_TIME

  @mock_messages(['liveLocationKalman'])
  def test_camera_procs(self):
    baseline = get_power()

    prev = baseline
    used = {}
    warmup_time = {}
    msg_counts = {}

    for proc in PROCS:
      managed_processes[proc.name].start()
      now, local_msg_counts, warmup_time[proc.name] = self.get_power_with_warmup_for_target(proc, prev)
      msg_counts.update(local_msg_counts)

      used[proc.name] = now - prev
      prev = now

    manager_cleanup()

    tab = [['process', 'expected (W)', 'measured (W)', '# msgs expected', '# msgs received', "warmup time (s)"]]
    for proc in PROCS:
      cur = used[proc.name]
      expected = proc.power
      msgs_received = sum(msg_counts[msg] for msg in proc.msgs)
      msgs_expected = self.get_expected_messages(proc)
      tab.append([proc.name, round(expected, 2), round(cur, 2), msgs_expected, msgs_received, warmup_time[proc.name]])
      with self.subTest(proc=proc.name):
        np.testing.assert_allclose(msgs_expected, msgs_received, rtol=.02, atol=2)
        np.testing.assert_allclose(cur, expected, rtol=proc.rtol, atol=proc.atol)
    print(tabulate(tab))
    print(f"Baseline {baseline:.2f}W\n")


if __name__ == "__main__":
  pytest.main(sys.argv)
