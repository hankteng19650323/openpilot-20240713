#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#define FRAME_BUF_COUNT 16

class FrameReader {
public:
  FrameReader(const std::string &url);
  bool process();
  ~FrameReader();
  uint8_t *get(int idx);
  int getRGBSize() const { return width * height * 3; }
  size_t getFrameCount() const { return frames_.size(); }
  bool valid() const { return valid_; }

  int width = 0, height = 0;

private:
  bool processFrames();
  void decodeThread();
  uint8_t *decodeFrame(AVPacket *pkt);

  struct Frame {
    AVPacket pkt = {};
    uint8_t *data = nullptr;
    bool failed = false;
  };
  std::vector<Frame> frames_;

  AVFormatContext *pFormatCtx_ = NULL;
  AVCodecContext *pCodecCtx_ = NULL;
  AVFrame *frmRgb_ = nullptr;
  std::queue<uint8_t *> buffer_pool;
  struct SwsContext *sws_ctx_ = NULL;

  std::mutex mutex_;
  std::condition_variable cv_decode_;
  std::condition_variable cv_frame_;
  int decode_idx_ = 0;
  std::atomic<bool> exit_ = false;
  bool valid_ = false;
  std::string url_;
  std::thread decode_thread_;
};
