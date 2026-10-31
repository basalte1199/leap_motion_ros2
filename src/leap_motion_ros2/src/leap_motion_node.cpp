#include <LeapC.h>
#include <dlfcn.h>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <rclcpp/rclcpp.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <geometry_msgs/msg/pose_array.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include "leap_motion_ros2/msg/tracking_frame.hpp"
using namespace std::chrono_literals;
namespace lm = leap_motion_ros2::msg;

// LeapC is loaded at runtime so a workspace can be built without the proprietary SDK.
class LeapApi {
  void *lib_ = nullptr;
  template<class T> T symbol(const char *name) {
    auto p = dlsym(lib_, name);
    if (!p) throw std::runtime_error(std::string("Missing LeapC symbol: ") + name);
    return reinterpret_cast<T>(p);
  }
public:
  decltype(&LeapCreateConnection) create{};
  decltype(&LeapOpenConnection) open{};
  decltype(&LeapPollConnection) poll{};
  decltype(&LeapDestroyConnection) destroy{};
  decltype(&LeapSetTrackingMode) mode{};
  decltype(&LeapSetPolicyFlags) policy{};
  explicit LeapApi(const std::string &path) {
    lib_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib_) throw std::runtime_error(std::string("Cannot load LeapC: ") + dlerror());
    try {
      create = symbol<decltype(create)>("LeapCreateConnection");
      open = symbol<decltype(open)>("LeapOpenConnection");
      poll = symbol<decltype(poll)>("LeapPollConnection");
      destroy = symbol<decltype(destroy)>("LeapDestroyConnection");
      mode = symbol<decltype(mode)>("LeapSetTrackingMode");
      policy = symbol<decltype(policy)>("LeapSetPolicyFlags");
    } catch (...) { dlclose(lib_); throw; }
  }
  ~LeapApi() { if (lib_) dlclose(lib_); }
};
geometry_msgs::msg::Point point(const LEAP_VECTOR &v) {
  geometry_msgs::msg::Point p; p.x=-v.z*0.001; p.y=-v.x*0.001; p.z=v.y*0.001; return p;
}
geometry_msgs::msg::Vector3 vector(const LEAP_VECTOR &v, double scale) {
  geometry_msgs::msg::Vector3 p; p.x=-v.z*scale; p.y=-v.x*scale; p.z=v.y*scale; return p;
}
geometry_msgs::msg::Quaternion rotation(const LEAP_QUATERNION &q) {
  // Fixed SDK -> ROS rotation multiplied by the SDK orientation.
  geometry_msgs::msg::Quaternion r;
  r.x=0.5*(q.w+q.x-q.z+q.y); r.y=0.5*(-q.w+q.y-q.x-q.z);
  r.z=0.5*(-q.w+q.z+q.y+q.x); r.w=0.5*(q.w-q.x+q.y+q.z); return r;
}
lm::Bone bone(const LEAP_BONE &b) {
  lm::Bone out; out.start=point(b.prev_joint); out.end=point(b.next_joint);
  out.width=b.width*0.001; out.orientation=rotation(b.rotation); return out;
}
class LeapNode : public rclcpp::Node {
  std::unique_ptr<LeapApi> api_;
  LEAP_CONNECTION connection_ = nullptr;
  bool connected_ = false;
  bool stale_ = true;
  std::string frame_;
  eLeapTrackingMode mode_;
  double stale_timeout_;
  std::chrono::steady_clock::time_point last_frame_ = std::chrono::steady_clock::now();
  rclcpp::Publisher<lm::TrackingFrame>::SharedPtr frames_;
  rclcpp::Publisher<geometry_msgs::msg::PoseArray>::SharedPtr palms_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr markers_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_;
  rclcpp::TimerBase::SharedPtr poll_timer_, status_timer_;
public:
  LeapNode() : Node("leap_motion") {
    frame_=declare_parameter("frame_id", "leap_motion");
    if (frame_.empty()) throw std::runtime_error("frame_id must not be empty");
    stale_timeout_=declare_parameter("stale_timeout", 1.0);
    if (!std::isfinite(stale_timeout_) || stale_timeout_ <= 0) throw std::runtime_error("stale_timeout must be positive");
    auto mode=declare_parameter("tracking_mode", "desktop");
    if (mode=="desktop") mode_=eLeapTrackingMode_Desktop;
    else if (mode=="hmd") mode_=eLeapTrackingMode_HMD;
    else if (mode=="screen_top") mode_=eLeapTrackingMode_ScreenTop;
    else throw std::runtime_error("tracking_mode must be desktop, hmd or screen_top");
    frames_=create_publisher<lm::TrackingFrame>("~/hands", rclcpp::SensorDataQoS());
    palms_=create_publisher<geometry_msgs::msg::PoseArray>("~/palms", rclcpp::SensorDataQoS());
    markers_=create_publisher<visualization_msgs::msg::MarkerArray>("~/markers", rclcpp::SensorDataQoS());
    diagnostics_=create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    api_=std::make_unique<LeapApi>(declare_parameter("library_path", "libLeapC.so"));
    if (api_->create(nullptr, &connection_) != eLeapRS_Success)
      throw std::runtime_error("LeapCreateConnection failed");
    if (api_->open(connection_) != eLeapRS_Success) {
      api_->destroy(connection_); connection_=nullptr;
      throw std::runtime_error("LeapOpenConnection failed");
    }
    poll_timer_=create_wall_timer(5ms, [this]{ poll(); });
    status_timer_=create_wall_timer(1s, [this]{ status(); });
    RCLCPP_INFO(get_logger(), "Waiting for Ultraleap service; publishing /leap_motion/hands");
  }
  ~LeapNode() override { if (connection_) api_->destroy(connection_); }
private:
  void poll() {
    for (int i=0; i<32; ++i) {
      LEAP_CONNECTION_MESSAGE event{};
      auto result=api_->poll(connection_, 0, &event);
      if (result==eLeapRS_Timeout) break;
      if (result!=eLeapRS_Success) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "LeapPollConnection: 0x%x", static_cast<unsigned>(result));
        break;
      }
      if (event.type==eLeapEventType_Connection) {
        connected_=true;
        auto policy_result=api_->policy(connection_, eLeapPolicyFlag_BackgroundFrames, 0);
        if (policy_result!=eLeapRS_Success) RCLCPP_WARN(get_logger(), "Background frames policy failed: 0x%x", static_cast<unsigned>(policy_result));
        auto r=api_->mode(connection_, mode_);
        if (r!=eLeapRS_Success) RCLCPP_WARN(get_logger(), "Tracking mode request failed: 0x%x", static_cast<unsigned>(r));
        RCLCPP_INFO(get_logger(), "Connected to Ultraleap service");
      } else if (event.type==eLeapEventType_ConnectionLost) {
        connected_=false; stale_=true; publish_empty();
        RCLCPP_WARN(get_logger(), "Ultraleap service disconnected; waiting for reconnect");
      } else if (event.type==eLeapEventType_Tracking && event.tracking_event) {
        last_frame_=std::chrono::steady_clock::now(); stale_=false;
        publish(*event.tracking_event);
      }
    }
    if (!stale_ && std::chrono::duration<double>(std::chrono::steady_clock::now()-last_frame_).count()>stale_timeout_) {
      stale_=true; publish_empty();
    }
  }
  void publish_empty() { LEAP_TRACKING_EVENT empty{}; publish(empty); }
  void publish(const LEAP_TRACKING_EVENT &f) {
    lm::TrackingFrame msg; msg.header.stamp=now(); msg.header.frame_id=frame_;
    msg.frame_id=f.tracking_frame_id; msg.device_timestamp_us=f.info.timestamp; msg.framerate=f.framerate;
    geometry_msgs::msg::PoseArray poses; poses.header=msg.header;
    visualization_msgs::msg::MarkerArray markers;
    visualization_msgs::msg::Marker clear; clear.action=clear.DELETEALL; markers.markers.push_back(clear);
    for (uint32_t i=0; i<f.nHands; ++i) {
      const auto &h=f.pHands[i]; lm::Hand hand;
      hand.id=h.id; hand.type=static_cast<uint8_t>(h.type); hand.confidence=h.confidence;
      hand.visible_time_us=h.visible_time; hand.palm.position=point(h.palm.position);
      hand.palm.orientation=rotation(h.palm.orientation); hand.palm_velocity=vector(h.palm.velocity, 0.001);
      hand.palm_normal=vector(h.palm.normal, 1); hand.direction=vector(h.palm.direction, 1);
      hand.palm_width=h.palm.width*0.001; hand.pinch_distance=h.pinch_distance*0.001;
      hand.pinch_strength=h.pinch_strength; hand.grab_strength=h.grab_strength; hand.grab_angle=h.grab_angle;
      hand.arm=bone(h.arm);
      visualization_msgs::msg::Marker lines; lines.header=msg.header; lines.ns="skeleton";
      lines.id=static_cast<int>(i); lines.type=lines.LINE_LIST; lines.action=lines.ADD;
      lines.pose.orientation.w=1; lines.scale.x=0.004; lines.color.a=1;
      lines.color.r=h.type==eLeapHandType_Left ? 0.2 : 1.0;
      lines.color.g=0.7; lines.color.b=h.type==eLeapHandType_Left ? 1.0 : 0.2;
      lines.lifetime=rclcpp::Duration::from_seconds(stale_timeout_);
      lines.points.push_back(hand.arm.start); lines.points.push_back(hand.arm.end);
      for (size_t d=0; d<5; ++d) {
        hand.fingers[d].id=h.digits[d].finger_id; hand.fingers[d].extended=h.digits[d].is_extended!=0;
        for (size_t b=0; b<4; ++b) {
          hand.fingers[d].bones[b]=bone(h.digits[d].bones[b]);
          lines.points.push_back(hand.fingers[d].bones[b].start);
          lines.points.push_back(hand.fingers[d].bones[b].end);
        }
      }
      poses.poses.push_back(hand.palm); msg.hands.push_back(hand); markers.markers.push_back(lines);
    }
    frames_->publish(msg); palms_->publish(poses); markers_->publish(markers);
  }
  void status() {
    diagnostic_msgs::msg::DiagnosticArray a; a.header.stamp=now();
    diagnostic_msgs::msg::DiagnosticStatus s; s.name="leap_motion/tracking"; s.hardware_id="Leap Motion";
    s.level=connected_ && !stale_ ? s.OK : s.WARN;
    s.message=!connected_ ? "Waiting for Ultraleap service" : stale_ ? "Service connected, no tracking frames" : "Tracking frames received";
    a.status.push_back(s); diagnostics_->publish(a);
    if (s.level!=s.OK) RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 10000, "%s", s.message.c_str());
  }
};
int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  try { rclcpp::spin(std::make_shared<LeapNode>()); }
  catch (const std::exception &e) {
    RCLCPP_ERROR(rclcpp::get_logger("leap_motion"), "%s", e.what());
    rclcpp::shutdown(); return 1;
  }
  rclcpp::shutdown(); return 0;
}
