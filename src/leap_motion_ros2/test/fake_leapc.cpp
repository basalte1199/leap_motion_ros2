// Test double only; never used by the normal launch file.
#include <LeapC.h>
#include <chrono>
static auto start=std::chrono::steady_clock::now();
static int phase=0;
static auto last=start;
extern "C" {
eLeapRS LeapCreateConnection(const LEAP_CONNECTION_CONFIG*, LEAP_CONNECTION *c) { *c=reinterpret_cast<LEAP_CONNECTION>(1); return eLeapRS_Success; }
eLeapRS LeapOpenConnection(LEAP_CONNECTION) { return eLeapRS_Success; }
eLeapRS LeapSetPolicyFlags(LEAP_CONNECTION, uint64_t, uint64_t) { return eLeapRS_Success; }
void LeapDestroyConnection(LEAP_CONNECTION) {}
eLeapRS LeapSetTrackingMode(LEAP_CONNECTION, eLeapTrackingMode) { return eLeapRS_Success; }
eLeapRS LeapPollConnection(LEAP_CONNECTION, uint32_t, LEAP_CONNECTION_MESSAGE *e) {
  auto now=std::chrono::steady_clock::now();
  if (phase==0) { e->type=eLeapEventType_Connection; phase=1; return eLeapRS_Success; }
  if (std::chrono::duration<double>(now-start).count()>4) return eLeapRS_Timeout;
  if (std::chrono::duration<double>(now-last).count()<0.02) return eLeapRS_Timeout;
  last=now;
  static LEAP_HAND hands[2]{};
  for (int i=0; i<2; ++i) {
    hands[i].id=42+i; hands[i].type=static_cast<eLeapHandType>(i);
    hands[i].palm.position.x=100; hands[i].palm.position.y=200; hands[i].palm.position.z=300;
    hands[i].palm.orientation.w=1; hands[i].pinch_distance=25; hands[i].pinch_strength=0.75;
    for (auto &digit:hands[i].digits) for (auto &b:digit.bones) {
      b.prev_joint.x=100; b.next_joint.y=200; b.rotation.w=1; b.width=10;
    }
  }
  static LEAP_TRACKING_EVENT frame{};
  frame.nHands=2; frame.pHands=hands; frame.tracking_frame_id++; frame.info.timestamp=123456; frame.framerate=50;
  e->type=eLeapEventType_Tracking; e->tracking_event=&frame; return eLeapRS_Success;
}
}
