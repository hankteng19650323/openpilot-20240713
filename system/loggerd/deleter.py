#!/usr/bin/env python3
import os
import shutil
import threading
from system.swaglog import cloudlog
from system.loggerd.config import ROOT, get_available_bytes, get_available_percent
from system.loggerd.uploader import listdir_by_creation
from system.loggerd.xattr_cache import getxattr

MIN_BYTES = 5 * 1024 * 1024 * 1024
MIN_PERCENT = 10

DELETE_LAST = ['boot', 'crash']

PRESERVE_ATTR_NAME = 'user.preserve'
PRESERVE_ATTR_VALUE = b'1'
PRESERVE_COUNT = 5


def has_preserve_xattr(d: str) -> bool:
  return getxattr(os.path.join(ROOT, d), PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE


def deleter_thread(exit_event):
  while not exit_event.is_set():
    out_of_bytes = get_available_bytes(default=MIN_BYTES + 1) < MIN_BYTES
    out_of_percent = get_available_percent(default=MIN_PERCENT + 1) < MIN_PERCENT

    if out_of_percent or out_of_bytes:
      # remove the earliest directory we can
      dirs = sorted(listdir_by_creation(ROOT), key=lambda x: x in DELETE_LAST)

      # skip deleting most recent N preserved segments (and their prior segment)
      preserved_dirs = []
      for n, d in enumerate(filter(has_preserve_xattr, reversed(dirs))):
        if n == PRESERVE_COUNT:
          break
        date_str, _, seg_num = d.rpartition("--")

        # preserve segment and its prior
        preserved_dirs.append(d)
        preserved_dirs.append(f"{date_str}--{int(seg_num) - 1}")

      for delete_dir in filter(lambda d: d not in preserved_dirs, dirs):
        delete_path = os.path.join(ROOT, delete_dir)

        if any(name.endswith(".lock") for name in os.listdir(delete_path)):
          continue

        try:
          cloudlog.info(f"deleting {delete_path}")
          if os.path.isfile(delete_path):
            os.remove(delete_path)
          else:
            shutil.rmtree(delete_path)
          break
        except OSError:
          cloudlog.exception(f"issue deleting {delete_path}")
      exit_event.wait(.1)
    else:
      exit_event.wait(30)


def main():
  deleter_thread(threading.Event())


if __name__ == "__main__":
  main()
