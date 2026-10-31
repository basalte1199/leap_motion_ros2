# 検証結果（2026-10-31）

- USB: `f182:0003 Leap Motion Controller` を確認。
- ROS 2 Jazzy / colcon: ビルド成功。
- SDK同梱のGemini 5.17.1.0ヘッダーでビルド。
- 模擬LeapC → ROS通信: 左右2手、位置・四元数・長さの変換、5指×4骨、
  MarkerArray、診断OK→WARN、受信停止時の空配列／表示クリアを確認。
- 実際のSDK `libLeapC.so` を読み込み、launch経由で起動・終了成功。
- ライブラリなし／負のタイムアウトの起動失敗を確認。
- `scripts/*.sh`: bash構文確認。
- SDK Debianパッケージ全体のtar読み取り成功、SHA-256記録済み。

ユーザーによるSDKサービス導入後、サービスとROSノードの稼働を確認。
8秒間で921フレーム（約115.3 Hz）を `/leap_motion/hands` で受信し、診断OKを確認。
その間の手の検出は0件。実際の手骨格の精度は未検証。
