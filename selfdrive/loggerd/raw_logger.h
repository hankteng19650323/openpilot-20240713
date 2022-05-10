#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include "selfdrive/loggerd/encoder.h"
#include "selfdrive/loggerd/loggerd.h"
#include "selfdrive/loggerd/video_writer.h"

class RawLogger : public VideoEncoder {
 public:
  RawLogger(const char* filename, CameraType type, int in_width, int in_height, int fps,
            int bitrate, Codec codec, int out_width, int out_height, bool write) :
            VideoEncoder(filename, type, in_width, in_height, fps, bitrate, RAW, out_width, out_height, write) { encoder_init(); }
  ~RawLogger();
  void encoder_init();
  int encode_frame(const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr,
                   int in_width_, int in_height_, VisionIpcBufExtra *extra);
  void encoder_open(const char* path);
  void encoder_close();

private:
  int counter = 0;
  bool is_open = false;

  AVCodecContext *codec_ctx;
  AVFrame *frame = NULL;
  std::vector<uint8_t> downscale_buf;
};
