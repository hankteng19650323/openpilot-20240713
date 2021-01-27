#include <cassert>
#include <sys/resource.h>

#include <ftw.h>
#include <algorithm>
#include "common/timing.h"
#include "common/params.h"
#include "common/swaglog.h"
#include "common/util.h"
#include "camerad/cameras/camera_common.h"
#include "logger.h"
#include "services.h"

#include "visionipc.h"
#include "visionipc_client.h"
#include "encoder.h"
#if defined(QCOM) || defined(QCOM2)
#include "omx_encoder.h"
#define Encoder OmxEncoder
#else
#include "raw_logger.h"
#define Encoder RawLogger
#endif

namespace {

constexpr int MAIN_FPS = 20;

#ifndef QCOM2
constexpr int MAIN_BITRATE = 5000000;
constexpr int DCAM_BITRATE = 2500000;
const bool IS_QCOM2 = false;
constexpr int DCAM_BITRATE = 2500000;
#else
constexpr int MAIN_BITRATE = 10000000;
constexpr int DCAM_BITRATE = MAIN_BITRATE;
const bool IS_QCOM2 = true;
#endif

#define NO_CAMERA_PATIENCE 500 // fall back to time-based rotation if all cameras are dead

const int SEGMENT_LENGTH = getenv("LOGGERD_TEST") ? atoi(getenv("LOGGERD_SEGMENT_LENGTH")) : 60;

ExitHandler do_exit;

LogCameraInfo cameras_logged[] = {
    .stream_type = VISION_STREAM_YUV_BACK,
    .frame_packet_name = "roadCameraState",
    .fps = MAIN_FPS,
    .bitrate = MAIN_BITRATE,
    .is_h265 = true,
    .downscale = false,
    .has_qcamera = true},
  {.id = D_CAMERA,
    .stream_type = VISION_STREAM_YUV_FRONT,
    .filename = "dcamera.hevc",
    .frame_packet_name = "driverCameraState",
    .fps = MAIN_FPS, // on EONs, more compressed this way
    .bitrate = DCAM_BITRATE,
    .is_h265 = true,
    .downscale = false,
    .has_qcamera = false},
#ifdef QCOM2
  {.id = E_CAMERA,
    .stream_type = VISION_STREAM_YUV_WIDE,
    .filename = "ecamera.hevc",
    .frame_packet_name = "wideRoadCameraState",
    .fps = MAIN_FPS,
    .bitrate = MAIN_BITRATE,
    .is_h265 = true,
    .downscale = false,
    .has_qcamera = false},
#endif
};
const LogCameraInfo qcam_info = {
    .filename = "qcamera.ts",
    .fps = MAIN_FPS,
    .bitrate = 256000,
    .is_h265 = false,
    .downscale = true,
#ifndef QCOM2
    .frame_width = 480, .frame_height = 360
#else
    .frame_width = 526, .frame_height = 330 // keep pixel count the same?
#endif
};


namespace {

typedef struct QlogState {
  int counter, freq;
} QlogState;

class EncoderState {
public:
  EncoderState(const LogCameraInfo &ci, SubSocket *sock, const QlogState &qs, bool need_waiting)
      : ci(ci), frame_sock(sock), qlog_state(qs), need_waiting(need_waiting) {}
  LogCameraInfo ci;
  std::unique_ptr<SubSocket> frame_sock;
  QlogState qlog_state;
  const bool need_waiting;
};

struct LoggerdState {
  LoggerState logger;
  char segment_path[4096];
  int encoders_max_waiting = 0;
  int rotate_segment;
  double last_rotate_tms;
  std::mutex rotate_lock;
  std::atomic<double> last_camera_seen_tms;
  std::atomic<int> encoders_waiting;
  std::condition_variable cv;
  std::vector<std::unique_ptr<EncoderState>> encoder_states;
};
LoggerdState s;

void drain_socket(LoggerHandle *lh, SubSocket *sock, QlogState &qs) {
  if (!lh) return;

  Message *msg = nullptr;
  while (!do_exit && (msg = sock->receive(true))) {
    lh_log(lh, (uint8_t *)msg->getData(), msg->getSize(), qs.counter == 0 && qs.freq != -1);
    if (qs.freq != -1) {
      qs.counter = (qs.counter + 1) % qs.freq;
    }
    delete msg;
  }
}

void encoder_thread(EncoderState *es) {
  set_thread_name(es->ci.filename);

  uint32_t total_frame_cnt = 0, segment_frame_cnt = 0;
  LoggerHandle *lh = NULL;
  std::vector<Encoder *> encoders;
  int encoder_segment = -1;
  std::string segment_path;
 
  VisionIpcClient vipc_client = VisionIpcClient("camerad", es->ci.stream_type, false);
  while (!do_exit) {
    if (!vipc_client.connect(false)){
      util::sleep_for(100);
      continue;
    }

    // init encoders
    if (encoders.empty()) {
      VisionBuf buf_info = vipc_client.buffers[0];
      LOGD("encoder init %dx%d", buf_info.width, buf_info.height);
      // main encoder
      encoders.push_back(new Encoder(es->ci.filename, buf_info.width, buf_info.height,
                                     es->ci.fps, es->ci.bitrate, es->ci.is_h265, es->ci.downscale));
      // qcamera encoder
      if (es->ci.has_qcamera) {
        encoders.push_back(new Encoder(qcam_info.filename, qcam_info.frame_width, qcam_info.frame_height,
                                       qcam_info.fps, qcam_info.bitrate, qcam_info.is_h265, qcam_info.downscale));
      }
    }

    while (!do_exit) {
      VisionIpcBufExtra extra;
      VisionBuf* buf = vipc_client.recv(&extra);
      if (buf == nullptr) continue;

      s.last_camera_seen_tms = millis_since_boot();
      bool should_rotate = false;
      {
        std::unique_lock lk(s.rotate_lock);
        // rotate the encoder if the logger is on a newer segment
        should_rotate = (encoder_segment != s.rotate_segment);
        if (!should_rotate && es->need_waiting && segment_frame_cnt == SEGMENT_LENGTH * MAIN_FPS) {
          // encoder need rotate
          should_rotate = true;
          s.encoders_waiting++;
          s.cv.wait(lk, [&] { return s.encoders_waiting == 0 || do_exit; });
          if (do_exit) break;
        }
        if (should_rotate) {
          encoder_segment = s.rotate_segment;
          segment_path = s.segment_path;
        }
      }
      if (should_rotate) {
        LOGW("camera %d rotate encoder to %s", es->ci.id, segment_path.c_str());
        if (lh) {
          lh_close(lh);
        }
        lh = logger_get_handle(&s.logger);
        for (auto &e : encoders) {
          e->encoder_close();
          e->encoder_open(segment_path.c_str());
        }
        segment_frame_cnt = 0;
      }

      // log frame socket
      drain_socket(lh, es->frame_sock.get(), es->qlog_state);
      // encode frame
      for (int i = 0; i < encoders.size() && !do_exit; ++i) {
        int out_id = encoders[i]->encode_frame(buf->y, buf->u, buf->v, buf->width, buf->height, extra.timestamp_eof);
        if (i > 0) continue;

        // publish encode index
        MessageBuilder msg;
        // this is really ugly
        auto eidx = es->ci.id == D_CAMERA ? msg.initEvent().initFrontEncodeIdx() :
                    (es->ci.id == E_CAMERA ? msg.initEvent().initWideEncodeIdx() : msg.initEvent().initEncodeIdx());
        eidx.setFrameId(extra.frame_id);
        eidx.setTimestampSof(extra.timestamp_sof);
        eidx.setTimestampEof(extra.timestamp_eof);
        eidx.setType((IS_QCOM2 || es->ci.id != D_CAMERA) ? cereal::EncodeIndex::Type::FULL_H_E_V_C : cereal::EncodeIndex::Type::FRONT);
        eidx.setEncodeId(total_frame_cnt);
        eidx.setSegmentNum(encoder_segment);
        eidx.setSegmentId(out_id);
        if (lh) {
          auto bytes = msg.toBytes();
          lh_log(lh, bytes.begin(), bytes.size(), false);
        }
      }
      ++segment_frame_cnt;
      ++total_frame_cnt;
    }

    if (lh) {
      lh_close(lh);
      lh = NULL;
    }
  }

  LOG("encoder destroy");
  for(auto &e : encoders) {
    e->encoder_close();
    delete e;
  }
}

static int clear_locks_fn(const char* fpath, const struct stat *sb, int tyupeflag) {
  const char* dot = strrchr(fpath, '.');
  if (dot && strcmp(dot, ".lock") == 0) {
    unlink(fpath);
  }
  return 0;
}

static void clear_locks() {
  ftw(LOG_ROOT.c_str(), clear_locks_fn, 16);
}

void loggerd_logger_next() {
  s.last_rotate_tms = millis_since_boot();
  assert(0 == logger_next(&s.logger, LOG_ROOT.c_str(), s.segment_path, sizeof(s.segment_path), &s.rotate_segment));
  LOGW((s.rotate_segment == 0) ? "logging to %s" : "rotated to %s", s.segment_path);
}

void loggerd_rotate() {
  bool new_segment = s.encoders_waiting >= s.encoders_max_waiting;
  if (!new_segment) {
    const double tms = millis_since_boot();
    if (((tms - s.last_rotate_tms) >= SEGMENT_LENGTH * 1000) && (tms - s.last_camera_seen_tms) >= NO_CAMERA_PATIENCE ) {
      new_segment = true;
      LOGW("no camera packet seen. auto rotated");
    }
  }
  // rotate to new segment
  if (new_segment && !do_exit) {
    {
      std::unique_lock lk(s.rotate_lock);
      loggerd_logger_next();
      s.encoders_waiting = 0;
    }
    s.cv.notify_all();
  }
}

} // namespace

int main(int argc, char** argv) {
  setpriority(PRIO_PROCESS, 0, -12);

  clear_locks();

  // setup messaging
  std::map<SubSocket*, QlogState> qlog_states;
  std::unique_ptr<Context> ctx(Context::create());
  std::unique_ptr<Poller> poller(Poller::create());
  const bool record_front = Params().read_db_bool("RecordFront");
  // subscribe to all socks
  for (const auto& it : services) {
    if (!it.should_log) continue;

    SubSocket * sock = SubSocket::create(ctx.get(), it.name);
    assert(sock != NULL);
    QlogState qs = {.counter = 0, .freq = it.decimation};

    auto cam = std::find_if(std::begin(cameras_logged), std::end(cameras_logged), [&](LogCameraInfo &ci) {
      return strcmp(it.name, ci.frame_packet_name) == 0 && (ci.id != D_CAMERA || record_front);
    });
    if (cam != std::end(cameras_logged)) {
      bool need_waiting = (IS_QCOM2 || cam->id != D_CAMERA);
      s.encoder_states.push_back(std::make_unique<EncoderState>(*cam, sock, qs, need_waiting));
      continue;
    }
    poller->registerSocket(sock);
    qlog_states[sock] = qs;
  }

  // init and open logger
  logger_init(&s.logger, "rlog", true);
  loggerd_logger_next();
  // start encoders
  std::vector<std::thread> encoder_threads;
  for (auto &es : s.encoder_states) {
    s.encoders_max_waiting += es->need_waiting;
    encoder_threads.push_back(std::thread(encoder_thread, es.get()));
  }
  // poll for new messages on all sockets
  while (!do_exit) {
    for (auto sock : poller->poll(1000)) {
      drain_socket(s.logger.cur_handle, sock, qlog_states[sock]);
    }
    loggerd_rotate();
  }

  LOGW("closing encoders");
  s.cv.notify_all();
  for (auto &[sock, qs] : qlog_states) delete sock;
  for (auto &t : encoder_threads) t.join();

  LOGW("closing logger");
  logger_close(&s.logger);
  return 0;
}
