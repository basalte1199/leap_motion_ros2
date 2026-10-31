# Leap Motion ROS 2 Jazzy wrapper

接続中の初代Leap Motion Controller (`f182:0003`) 用のLeapCラッパーです。
Ubuntu 24.04 / ROS 2 Jazzy。ビルドにはSDKの `LeapC.h`、実機の認識には
Ultraleap Geminiサービスと `libLeapC.so` が必要です。SDK本体はリポジトリに含めません。

## 初回セットアップ

```bash
git clone https://github.com/basalte1199/leap_motion_ros2.git ~/leap_motion_ws
cd ~/leap_motion_ws
./scripts/install_service.sh
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install
```

sudoパスワードが必要です。SDKのダウンロード（未取得時）とインストール、対象USBだけのudevルール追加、
サービスの有効化と起動を行います。USB権限が反映されないときは抜き差ししてください。
SDKはGemini 5.17.1.0の公式APT配布物のInternet Archive保存版です。
公式配布ホスト `repo.ultraleap.com` は作成時に名前解決できませんでした。
パッケージは `vendor/service.deb` に保存します。SDKの利用条件はSDK同梱の規約に従います。

## 起動

```bash
~/leap_motion_ws/scripts/run.sh
```

機器を机上に置き、上方に手をかざしてください。ノードだけでは認識処理は行わず、
Ultraleapサービスから追跡結果を受け取ります。

```bash
# 別ターミナルでデータを確認
source /opt/ros/jazzy/setup.bash
source ~/leap_motion_ws/install/setup.bash
ros2 topic echo /leap_motion/hands --qos-reliability best_effort

# RVizで左右の骨格を表示
rviz2 -d ~/leap_motion_ws/src/leap_motion_ros2/config/leap_motion.rviz
```

| トピック | 型 | 内容 |
|---|---|---|
| `/leap_motion/hands` | `leap_motion_ros2/msg/TrackingFrame` | 全ての手、掌姿勢・速度、指5本×骨4本、前腕、pinch/grab |
| `/leap_motion/palms` | `geometry_msgs/msg/PoseArray` | 掌姿勢（左右・IDとの対応はhandsで確認） |
| `/leap_motion/markers` | `visualization_msgs/msg/MarkerArray` | 左青・右橙の骨格 |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | サービス接続と追跡フレームの受信状態 |

追跡トピックはSensorDataQoS（best effort / volatile）です。
手を検出しないフレームも空配列で配信します。フレーム停止が1秒続くかサービスが切断されると、
空配列と表示クリアを一度配信します。再接続はLeapCが行います。
マーカーにも有効期限を設定しています。空配列の代替フレームはID・デバイス時刻・レートが0です。
診断のOKはフレーム受信を表し、手の検出や認識精度を保証するものではありません。

位置・長さはm、速度はm/s、角度はradです。
センサーを基準にx前方・y左・z上で、SDKの `(x, y, z)` を `(-z, -x, y)` に変換します。
四元数、速度、法線、方向も同じ座標変換を適用します。
掌や骨の四元数のローカル軸定義はLeapCのままです（掌は `{normal × direction, -normal, -direction}`）。
`header.stamp` はROS受信時刻で、`device_timestamp_us` はSDKの単調時計のマイクロ秒です。
`frame_id` の既定値は `leap_motion`。TFは配信しないため、ロボットへ取り付ける場合は
実際の取り付け位置のstatic TFを別途設定してください。

## パラメータ

```bash
./scripts/run.sh tracking_mode:=desktop frame_id:=leap_motion
# SDKが別の場所にある場合
./scripts/run.sh library_path:=/absolute/path/libLeapC.so
```

`tracking_mode`: `desktop` / `hmd` / `screen_top`。
`stale_timeout`: 正の秒数、既定1.0（ノードパラメータ）。パラメータは起動時に読み取ります。
生IR画像、複数デバイス選択、認識モデル自体の実装は含みません。

## ビルド・検証

```bash
source /opt/ros/jazzy/setup.bash
cd ~/leap_motion_ws
colcon build --symlink-install
source install/setup.bash
ROS_DOMAIN_ID=176 python3 src/leap_motion_ros2/test/integration.py
```

統合テストは一時的な模擬LeapCライブラリを使い、実際のROS通信を通じて左右の手、
SI単位・座標・姿勢変換、指骨格、マーカー、診断、追跡タイムアウトを検証します。
通常の起動では模擬SDKを使用しません。実機ではサービス導入後に約115 HzでROSフレームを受信できることを確認済みです。
その確認中は手の検出は0件で、実際の手骨格の精度は未検証です。

## トラブルシューティング

```bash
systemctl status ultraleap-hand-tracking-service
journalctl -u ultraleap-hand-tracking-service -n 50 --no-pager
lsusb -d f182:0003
```

`Cannot load LeapC` はライブラリ未導入かパスの問題です。
`Waiting for Ultraleap service` はサービス未起動、`no tracking frames` はUSBアクセス権・接続・サービスログを確認してください。

## 参照

- LeapC: https://docs.ultraleap.com/api-reference/tracking-api/leapc-guide.html
- Linux SDK: https://docs.ultraleap.com/linux/index.html
- 配布パッケージ保存先: https://web.archive.org/web/20260124060130id_/https://repo.ultraleap.com/apt/pool/main/u/ultraleap-hand-tracking-service/ultraleap-hand-tracking-service_5.17.1.0-a9f25232-1.0_amd64.deb
