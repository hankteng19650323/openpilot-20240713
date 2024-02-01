#!/usr/bin/env python3
import time
import threading
import unittest
from collections import namedtuple
from pathlib import Path
from typing import Sequence

import openpilot.system.loggerd.deleter as deleter
from openpilot.common.timeout import Timeout, TimeoutException
from openpilot.system.loggerd.tests.loggerd_tests_common import UploaderTestCase

Stats = namedtuple("Stats", ['f_bavail', 'f_blocks', 'f_frsize'])


class TestDeleter(UploaderTestCase):
  def fake_statvfs(self, d):
    return self.fake_stats

  def setUp(self):
    self.f_type = "fcamera.hevc"
    super().setUp()
    self.fake_stats = Stats(f_bavail=0, f_blocks=10, f_frsize=4096)
    deleter.os.statvfs = self.fake_statvfs

  def start_thread(self):
    self.end_event = threading.Event()
    self.del_thread = threading.Thread(target=deleter.deleter_thread, args=[self.end_event])
    self.del_thread.daemon = True
    self.del_thread.start()

  def join_thread(self):
    self.end_event.set()
    self.del_thread.join()

  def test_delete(self):
    f_path = self.make_file_with_data(self.seg_dir, self.f_type, 1)

    self.start_thread()

    try:
      with Timeout(2, "Timeout waiting for file to be deleted"):
        while f_path.exists():
          time.sleep(0.01)
    finally:
      self.join_thread()

  def get_delete_order(self, f_paths: Sequence[Path], timeout: int = 5):
    deleted_order = []

    self.start_thread()
    try:
      with Timeout(timeout, "Timeout waiting for files to be deleted"):
        while True:
          for f in f_paths:
            if not f.exists() and f not in deleted_order:
              deleted_order.append(f)
          if len(deleted_order) == len(f_paths):
            break
          time.sleep(0.01)
    except TimeoutException:
      print("Not deleted:", [f for f in f_paths if f not in deleted_order])
      raise
    finally:
      self.join_thread()

    return deleted_order

  def assertDeleteOrder(self, f_paths: Sequence[Path], timeout: int = 5) -> None:
    self.assertEqual(
      self.get_delete_order(f_paths, timeout),
      f_paths,
      "Files not deleted in expected order"
    )

  def test_delete_order(self):
    self.assertDeleteOrder([
      self.make_file_with_data(self.seg_format.format(0), self.f_type),
      self.make_file_with_data(self.seg_format.format(1), self.f_type),
      self.make_file_with_data(self.seg_format2.format(0), self.f_type),
    ])

  def test_delete_both_files_and_dirs(self):
    self.assertDeleteOrder([
      self.make_file_with_data(f_dir="", fn="some_file"),
      self.make_file_with_data(f_dir="some_dir", fn="some_file"),
      self.make_file_with_data(f_dir=self.seg_format.format(0), fn=self.f_type),
      self.make_file_with_data(f_dir=self.seg_format.format(1), fn=self.f_type),
      self.make_file_with_data(f_dir=self.seg_format2.format(0), fn=self.f_type),
      self.make_file_with_data(f_dir=self.seg_format2.format(1), fn=self.f_type)
      ])

  def test_group_delete_order(self):
    created = [
      [self.make_file_with_data(f_dir="", fn="file_1"),
        self.make_file_with_data(f_dir="", fn="file_2"),
        self.make_file_with_data(f_dir="", fn="file_3")],
      [self.make_file_with_data(f_dir="dir_1", fn="nested_file"),
        self.make_file_with_data(f_dir="dir_2", fn="nested_file"),
        self.make_file_with_data(f_dir="dir_3", fn="nested_file")],
      [self.make_file_with_data(f_dir=self.seg_format.format(0), fn=self.f_type),
        self.make_file_with_data(f_dir=self.seg_format.format(1), fn=self.f_type),
        self.make_file_with_data(f_dir=self.seg_format2.format(0), fn=self.f_type)],
      ]

    flattened = [item for group in created for item in group]
    delete_order = self.get_delete_order(flattened)

    start = 0
    for group in created:
      end = start + len(group)
      deleted = delete_order[start:end]
      self.assertCountEqual(group, deleted)
      start = end

  def test_delete_many_preserved(self):
    self.assertDeleteOrder([
      self.make_file_with_data(self.seg_format.format(0), self.f_type),
      self.make_file_with_data(self.seg_format.format(1), self.f_type, preserve_xattr=deleter.PRESERVE_ATTR_VALUE),
      self.make_file_with_data(self.seg_format.format(2), self.f_type),
    ] + [
      self.make_file_with_data(self.seg_format2.format(i), self.f_type, preserve_xattr=deleter.PRESERVE_ATTR_VALUE)
      for i in range(5)
    ])

  def test_delete_last(self):
    self.assertDeleteOrder([
      self.make_file_with_data(self.seg_format.format(1), self.f_type),
      self.make_file_with_data(self.seg_format2.format(0), self.f_type),
      self.make_file_with_data(self.seg_format.format(0), self.f_type, preserve_xattr=deleter.PRESERVE_ATTR_VALUE),
      self.make_file_with_data("boot", self.seg_format[:-4]),
      self.make_file_with_data("crash", self.seg_format2[:-4]),
    ])

  def test_no_delete_when_available_space(self):
    f_path = self.make_file_with_data(self.seg_dir, self.f_type)

    block_size = 4096
    available = (10 * 1024 * 1024 * 1024) / block_size  # 10GB free
    self.fake_stats = Stats(f_bavail=available, f_blocks=10, f_frsize=block_size)

    self.start_thread()
    start_time = time.monotonic()
    while f_path.exists() and time.monotonic() - start_time < 2:
      time.sleep(0.01)
    self.join_thread()

    self.assertTrue(f_path.exists(), "File deleted with available space")

  def test_no_delete_with_lock_file(self):
    f_path = self.make_file_with_data(self.seg_dir, self.f_type, lock=True)

    self.start_thread()
    start_time = time.monotonic()
    while f_path.exists() and time.monotonic() - start_time < 2:
      time.sleep(0.01)
    self.join_thread()

    self.assertTrue(f_path.exists(), "File deleted when locked")


if __name__ == "__main__":
  unittest.main()
