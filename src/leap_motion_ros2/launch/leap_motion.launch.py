from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('frame_id', default_value='leap_motion'),
        DeclareLaunchArgument('tracking_mode', default_value='desktop'),
        DeclareLaunchArgument('library_path', default_value='libLeapC.so'),
        Node(package='leap_motion_ros2', executable='leap_motion_node',
             name='leap_motion', output='screen', parameters=[{
                 'frame_id': LaunchConfiguration('frame_id'),
                 'tracking_mode': LaunchConfiguration('tracking_mode'),
                 'library_path': LaunchConfiguration('library_path'),
             }]),
    ])
