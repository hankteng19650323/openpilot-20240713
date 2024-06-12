#!/usr/bin/env python3
import argparse
import os
import subprocess
import requests

from collections import defaultdict

from openpilot.common.basedir import BASEDIR
from openpilot.selfdrive.car.docs_definitions import Column
from openpilot.common.conversions import Conversions as CV

STAR_ICON = '<a href="##"><img valign="top" ' + \
            'src="https://media.githubusercontent.com/media/commaai/openpilot/master/docs/assets/icon-star-{}.svg" width="22" /></a>'
COLUMNS = "|" + "|".join([column.value for column in Column]) + "|"
COLUMN_HEADER = "|---|---|---|{}|".format("|".join([":---:"] * (len(Column) - 3)))
ARROW_SYMBOL = "➡️"

def download_file(url, filename):
  response = requests.get(url)
  with open(filename, 'wb') as file:
    file.write(response.content)


def column_change_format(line1, line2):
  info1, info2 = line1.split('|'), line2.split('|')
  return "|".join([f"{i1} {ARROW_SYMBOL} {i2}|" if i1 != i2 else f"{i1}|" for i1, i2 in zip(info1, info2, strict=True)])

def get_detail_sentence(data):
  make, model, package, longitudinal, fsr_longitudinal, fsr_steering, steering_torque, auto_resume, hardware, video = data.split("|")
  min_steer_speed, min_enable_speed = fsr_steering / CV.MS_TO_MPH, fsr_longitudinal / CV.MS_TO_MPH # default values

  sentence_builder = "openpilot upgrades your <strong>{car_model}</strong> with automated lane centering{alc} and adaptive cruise control{acc}."
  if min_steer_speed > min_enable_speed:
    alc = f" <strong>above {min_steer_speed * CV.MS_TO_MPH:.0f} mph</strong>," if min_steer_speed > 0 else " <strong>at all speeds</strong>,"
  else:
    alc = ""
  acc = ""
  if min_enable_speed > 0:
    acc = f" <strong>while driving above {min_enable_speed * CV.MS_TO_MPH:.0f} mph</strong>"
  elif auto_resume:
    acc = " <strong>that automatically resumes from a stop</strong>"
  if steering_torque != STAR_ICON:
    sentence_builder += " This car may not be able to take tight turns on its own."

  # experimental mode
  # TODO: Add experimental data
  # openpilotLongitudinalControl = True
  # experimentalLongitudinalAvailable = False
  # exp_link = "<a href='https://blog.comma.ai/090release/#experimental-mode' target='_blank' class='link-light-new-regular-text'>Experimental mode</a>"
  # if openpilotLongitudinalControl and not experimentalLongitudinalAvailable:
  #   sentence_builder += f" Traffic light and stop sign handling is also available in {exp_link}."

  return sentence_builder.format(car_model=f"{make} {model}", alc=alc, acc=acc)

def process_detail_sentences(info):
  detail_sentences = []
  ind = info.index("---")
  for line1, line2 in zip(info[1:ind], info[ind+1:], strict=True):
    name = ' '.join(line1[:2].split("|")[:2])
    detail1 = get_detail_sentence(line1[2:])
    detail2 = get_detail_sentence(line2[2:])
    if detail1 != detail2:
      detail_sentences.append(f"- Sentence for {name} changed!\n" +
                                 "  ```diff\n" +
                                 f"  - {detail1}\n" +
                                 f"  + {detail2}\n" +
                                 "  ```")
  return detail_sentences

def process_diff_information(info):
  header = info[0]
  category = None
  final_strings = []
  if "c" in header:
    category = "column"
    ind = info.index("---")
    for line1, line2 in zip(info[1:ind], info[ind+1:], strict=True):
      final_strings.append(column_change_format(line1[2:], line2[2:]))
  else:
    category = "additions" if "a" in header else "removals"
    final_strings = [x[2:] for x in info[1:]]
  return category, final_strings

def print_markdown(changes):
  markdown_builder = ["### ⚠️ This PR makes changes to [CARS.md](../blob/master/docs/CARS.md) ⚠️"]
  for title, category in (("## 🔀 Column Changes", "column"), ("## ❌ Removed", "removals"),
                          ("## ➕ Added", "additions"), ("## 📖 Detail Sentence Changes", "detail")):
    # TODO: Add details for detail changes
    if len(changes[category]):
      markdown_builder.append(title)
      if category not in ("detail",):
        markdown_builder.append(COLUMNS)
        markdown_builder.append(COLUMN_HEADER)
      markdown_builder.extend(changes[category])
  print("\n".join(markdown_builder))

def print_car_docs_diff(path):
  CARS_MD_OUT = os.path.join(BASEDIR, "docs", "CARS.md")
  changes = subprocess.run(['diff', path, CARS_MD_OUT], capture_output=True, text=True).stdout.split('\n')

  changes_markdown = defaultdict(list)
  ind = 0
  while ind < len(changes):
    if changes[ind] and changes[ind][0].isdigit():
      start = ind
      ind += 1
      while ind < len(changes) and changes[ind] and not changes[ind][0].isdigit():
        ind += 1
      category, strings = process_diff_information(changes[start:ind])
      changes_markdown[category] += strings
      if category == "column":
        changes_markdown["detail"] += process_detail_sentences(changes[start:ind])
    else:
      ind += 1

  if any(len(c) for c in changes_markdown.values()):
    print_markdown(changes_markdown)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--path", required=True)
  args = parser.parse_args()
  print_car_docs_diff(args.path)
