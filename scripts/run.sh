#!/usr/bin/env bash
set -eo pipefail
workspace="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source /opt/ros/jazzy/setup.bash
source "$workspace/install/setup.bash"
# Older Linux packages do not always register LeapC in ldconfig.
for location in /usr/lib/ultraleap-hand-tracking-service /usr/share/doc/ultraleap-hand-tracking-service; do
  if [[ -f "$location/libLeapC.so" ]]; then
    export LD_LIBRARY_PATH="$location:${LD_LIBRARY_PATH:-}"
  fi
done
exec ros2 launch leap_motion_ros2 leap_motion.launch.py "$@"
