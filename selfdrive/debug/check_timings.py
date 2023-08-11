#!/usr/bin/env python3

import sys
import time
from collections import defaultdict, deque
from typing import DefaultDict, MutableSequence

import numpy as np

import cereal.messaging as messaging

socks = {s: messaging.sub_sock(s, conflate=False) for s in sys.argv[1:]}
ts: DefaultDict[str, MutableSequence[float]] = defaultdict(lambda: deque(maxlen=100))

if __name__ == "__main__":
  while True:
    print()
    for s, sock in socks.items():
      msgs = messaging.drain_sock(sock)
      for m in msgs:
        ts[s].append(m.logMonoTime / 1e6)

      if len(ts[s]) > 2:
        d = np.diff(ts[s])
        print(f"{s:25} {np.mean(d):.2f} {np.std(d):.2f} {np.max(d):.2f} {np.min(d):.2f}")
    time.sleep(1)
