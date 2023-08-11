import os

from system.loggerd.config import ROOT
from system.loggerd.uploader import UPLOAD_ATTR_NAME, UPLOAD_ATTR_VALUE

for folder in os.walk(ROOT):
  for file1 in folder[2]:
    full_path = os.path.join(folder[0], file1)
    os.setxattr(full_path, UPLOAD_ATTR_NAME, UPLOAD_ATTR_VALUE)
