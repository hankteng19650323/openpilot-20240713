#!/usr/bin/env python3
import argparse
import json
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import sys
from collections import defaultdict

from openpilot.tools.lib.logreader import LogReader

DEMO_ROUTE = "9f583b1d93915c31|2022-05-18--10-49-51--0"

COLORS = ['blue', 'green', 'red', 'yellow', 'orange', 'purple']
PLOT_SERVICES = ['card', 'controlsd']  # , 'boardd']


def plot(lr):
  seen = set()
  aligned = False

  start_time = None
  # dict of services to events per inferred frame
  times = {s: [[]] for s in PLOT_SERVICES}

  first_event = None
  # temp_times = {s: [] for s in PLOT_SERVICES}  # holds only current frame of services

  timestamps = [json.loads(msg.logMessage) for msg in lr if msg.which() == 'logMessage' and 'timestamp' in msg.logMessage]
  # print(timestamps)
  timestamps = sorted(timestamps, key=lambda m: float(m['msg']['timestamp']['time']))

  # closely matches timestamp time
  start_time = next(msg.logMonoTime for msg in lr)

  for jmsg in timestamps:
    if len(times[PLOT_SERVICES[0]]) > 400:
      continue

    # print()
    # print(msg.logMonoTime)
    time = int(jmsg['msg']['timestamp']['time'])
    service = jmsg['ctx']['daemon']
    event = jmsg['msg']['timestamp']['event']
    # print(jmsg)
    # print(seen)

    if service in PLOT_SERVICES and first_event is None:
      first_event = event

    # Align the best we can; wait for all to be seen and this is the first event
    # TODO: detect first logMessage correctly by keeping track of events before aligned
    aligned = aligned or (all(s in seen for s in PLOT_SERVICES) and event == first_event)
    if not aligned:
      seen.add(service)
      continue

    if service in PLOT_SERVICES:

      # new frame when we've seen this event before
      new_frame = event in {e[1] for e in times[service][-1]}
      if new_frame:
        times[service].append([])

      # print(msg.logMonoTime, jmsg)
      print('new_frame', new_frame)
      times[service][-1].append(((time - start_time) * 1e-6, event))

  points = {"x": [], "y": [], "labels": []}
  height = 0.9
  offsets = [[0, -10 * j] for j in range(len(PLOT_SERVICES))]

  fig, ax = plt.subplots()

  for idx, service_times in enumerate(zip(*times.values())):
    print()
    print('idx', idx)
    service_bars = []
    for j, (service, frame_times) in enumerate(zip(times.keys(), service_times)):
      if idx + 1 == len(times[service]):
        break
      print(service, frame_times)
      start = frame_times[0][0]
      # use the first event time from next frame
      end = times[service][idx + 1][0][0]  # frame_times[-1][0]
      print('start, end', start, end)
      service_bars.append((start, end - start))
      for event in frame_times:
        points['x'].append(event[0])
        points['y'].append(idx - j * 1)
        points['labels'].append(event[1])
    print(service_bars)

    # offset = offset_services
    # offset each service
    for j, sb in enumerate(service_bars):
      ax.broken_barh([sb], (idx - height / 2 - j * 1, height), facecolors=[COLORS[j]], alpha=0.5)  # , offsets=offsets)
    # ax.broken_barh(service_bars, (idx - height / 2, height), facecolors=COLORS, alpha=0.5, offsets=offsets)

  scatter = ax.scatter(points['x'], points['y'], marker='d', edgecolor='black')
  txt = ax.text(0, 0, '', ha='center', fontsize=8, color='red')
  ax.set_xlabel('milliseconds')

  plt.legend(handles=[mpatches.Patch(color=COLORS[i], label=PLOT_SERVICES[i]) for i in range(len(PLOT_SERVICES))])

  def hover(event):
    txt.set_text("")
    status, pts = scatter.contains(event)
    txt.set_visible(status)
    if status:
      txt.set_text(points['labels'][pts['ind'][0]])
      txt.set_position((event.xdata, event.ydata + 1))
    event.canvas.draw()

  fig.canvas.mpl_connect("motion_notify_event", hover)

  plt.show()
  # plt.pause(1000)
  return times, points


if __name__ == "__main__":
  # parser = argparse.ArgumentParser(description="A tool for analyzing openpilot's end-to-end latency",
  #                                  formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  # parser.add_argument("--demo", action="store_true", help="Use the demo route instead of providing one")
  # parser.add_argument("route_or_segment_name", nargs='?', help="The route to print")
  #
  # if len(sys.argv) == 1:
  #   parser.print_help()
  #   sys.exit()
  # args = parser.parse_args()

  # r = DEMO_ROUTE if args.demo else args.route_or_segment_name.strip()
  # lr = LogReader(r, sort_by_time=True)
  lr = LogReader('08e4c2a99df165b1/00000016--c3a4ca99ec/0', sort_by_time=True)  # normal
  # lr = LogReader('08e4c2a99df165b1/00000017--e2d24ab118/0', sort_by_time=True)  # polls on carControl
  # lr = LogReader('08e4c2a99df165b1/00000018--cf65e47c24/0', sort_by_time=True)  # polls on carControl, sends it earlier
  # lr = LogReader('08e4c2a99df165b1/00000019--e73e3ab4df/0', sort_by_time=True)  # polls on carControl, more logging

  times, points = plot(lr)
