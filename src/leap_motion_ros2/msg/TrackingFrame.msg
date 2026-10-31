# SI units; header.stamp is ROS receipt time, not the SDK monotonic clock.
# ROS axes: x forward, y left, z up. SDK mapping: (-z, -x, y).
std_msgs/Header header
int64 frame_id
int64 device_timestamp_us
float32 framerate
leap_motion_ros2/Hand[] hands
