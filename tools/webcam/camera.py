import cv2 as cv
import numpy as np

class Camera:
  def __init__(self, camera_id):
    self.cam = cv.VideoCapture(camera_id)
    self.W = self.cam.get(cv.CAP_PROP_FRAME_WIDTH)
    self.H = self.cam.get(cv.CAP_PROP_FRAME_HEIGHT)

  @classmethod
  def bgr2nv12(self, bgr):
    yuv = cv.cvtColor(bgr, cv.COLOR_BGR2YUV_I420)
    uv_row_cnt = yuv.shape[0] // 3
    uv_plane = np.transpose(yuv[uv_row_cnt * 2:].reshape(2, -1), [1, 0])
    yuv[uv_row_cnt * 2:] = uv_plane.reshape(uv_row_cnt, -1)
    return yuv

  def read_frames(self):
    while True:
      sts , frame = self.cam.read()
      if not sts:
        break
      yuv = Camera.bgr2nv12(frame)
      yield yuv.tobytes()
    self.cam.release()
    cv.destroyAllWindows()
