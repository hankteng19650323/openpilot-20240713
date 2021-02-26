#!/usr/bin/env python3
import os
import signal
import subprocess
import time
import unittest

from common.basedir import BASEDIR
from common.timeout import Timeout
from selfdrive.test.openpilotci import get_url

class TestPlotJuggler(unittest.TestCase):

  def test_install(self):
    exit_code = os.system(os.path.join(BASEDIR, "tools/plotjuggler/install.sh"))
    self.assertEqual(exit_code, 0)

  def test_run(self):

    test_url = get_url("ffccc77938ddbc44|2021-01-04--16-55-41", 0)

    # Launch PlotJuggler with the executable in the bin directory
    p = subprocess.Popen(f'QT_QPA_PLATFORM=offscreen {os.path.join(BASEDIR, "tools/plotjuggler/juggle.py")} \
    "{test_url}" --bin_path={os.path.join(BASEDIR, "tools/plotjuggler/bin")} ', stderr=subprocess.PIPE ,shell=True,
    start_new_session=True)

    # Wait max 60 seconds for the "Done reading Rlog data" signal from the plugin
    with Timeout(60):
      while True:
          output = p.stderr.readline()
          if output == b'Done reading Rlog data\n':
            break

    # See if the process is still running after 15 seconds
    time.sleep(15)
    self.assertEqual(p.poll(), None)
    os.killpg(os.getpgid(p.pid), signal.SIGTERM)

if __name__ == "__main__":
  unittest.main()
