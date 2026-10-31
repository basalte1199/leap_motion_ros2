"""Run with sourced ROS workspace; verifies ROS serialization and stale clearing."""
import math
import os
import signal
from pathlib import Path
from ament_index_python.packages import get_package_prefix
import subprocess
import tempfile
import time
import rclpy
from rclpy.qos import qos_profile_sensor_data
from leap_motion_ros2.msg import TrackingFrame
from visualization_msgs.msg import MarkerArray
from diagnostic_msgs.msg import DiagnosticArray

package = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    lib = str(Path(tmp) / 'libFakeLeapC.so')
    subprocess.run(['g++', '-std=c++17', '-shared', '-fPIC', '-I' + str(package / 'include'),
                    str(package / 'test/fake_leapc.cpp'), '-o', lib], check=True)
    rclpy.init()
    node = rclpy.create_node('leap_wrapper_test')
    frames, markers, diagnostics = [], [], []
    subscriptions = [
        node.create_subscription(TrackingFrame, '/leap_motion/hands', frames.append, qos_profile_sensor_data),
        node.create_subscription(MarkerArray, '/leap_motion/markers', markers.append, qos_profile_sensor_data),
        node.create_subscription(DiagnosticArray, '/diagnostics', diagnostics.append, 10),
    ]
    executable = str(Path(get_package_prefix('leap_motion_ros2')) / 'lib/leap_motion_ros2/leap_motion_node')
    process = subprocess.Popen([executable,
                                '--ros-args', '-p', 'library_path:=' + lib], start_new_session=True)
    try:
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.1)
        assert process.poll() is None, 'node exited unexpectedly'
        active = next(f for f in frames if len(f.hands) == 2)
        assert [h.type for h in active.hands] == [0, 1]
        h = active.hands[0]
        assert math.isclose(h.palm.position.x, -0.3, abs_tol=1e-6)
        assert math.isclose(h.palm.position.y, -0.1, abs_tol=1e-6)
        assert math.isclose(h.palm.position.z, 0.2, abs_tol=1e-6)
        assert math.isclose(h.pinch_distance, 0.025, abs_tol=1e-6)
        assert h.pinch_strength == 0.75
        assert len(h.fingers) == 5 and len(h.fingers[0].bones) == 4
        q = h.palm.orientation
        assert (q.x, q.y, q.z, q.w) == (0.5, -0.5, -0.5, 0.5)
        assert active.device_timestamp_us == 123456
        assert active.header.frame_id == 'leap_motion'
        assert len(frames[-1].hands) == 0, 'stale hands were not cleared'
        assert len(markers[-1].markers) == 1 and markers[-1].markers[0].action == 3
        assert any(d.status[0].level == d.status[0].OK for d in diagnostics)
        assert diagnostics[-1].status[0].level == diagnostics[-1].status[0].WARN
        print('PASS: two hands, SI/axis/quaternion conversion, fingers, markers, diagnostics, stale clearing')
    finally:
        os.killpg(process.pid, signal.SIGINT)
        process.wait(timeout=5)
        node.destroy_node()
        rclpy.shutdown()
