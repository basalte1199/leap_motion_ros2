#!/usr/bin/env bash
set -euo pipefail
workspace="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
package="$workspace/vendor/service.deb"
if [[ ! -f "$package" ]]; then
  mkdir -p "$workspace/vendor"
  curl -fL --retry 3 -o "$package.download" 'https://web.archive.org/web/20260124060130id_/https://repo.ultraleap.com/apt/pool/main/u/ultraleap-hand-tracking-service/ultraleap-hand-tracking-service_5.17.1.0-a9f25232-1.0_amd64.deb'
  mv "$package.download" "$package"
fi
if [[ ! -f "$package" ]] || [[ "$(stat -c %s "$package")" != 698539986 ]] || ! dpkg-deb --info "$package" >/dev/null 2>&1; then
  echo 'SDKパッケージが未取得または不完全です。vendor/service.debを確認してください。' >&2
  exit 1
fi
expected_sha256=9d9162e4d133acf7f430c0e8127cc33c82da2cbb700f1ee04edeff5fab98e0e4
actual_sha256="$(sha256sum "$package" | cut -d ' ' -f 1)"
if [[ "$actual_sha256" != "$expected_sha256" ]]; then
  echo 'SDKパッケージのチェックサムが一致しません。' >&2
  exit 1
fi
# Ask for sudo before making system changes; never store the password.
sudo -v
sudo apt-get install -y "$package"
sudo usermod -a -G plugdev leap
sudo tee /etc/udev/rules.d/99-leap-motion-ros2.rules >/dev/null <<'RULE'
SUBSYSTEM=="usb", ATTR{idVendor}=="f182", ATTR{idProduct}=="0003", MODE="0660", GROUP="plugdev", TAG+="uaccess"
RULE
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb --action=add
sudo systemctl enable --now ultraleap-hand-tracking-service
sudo systemctl restart ultraleap-hand-tracking-service
systemctl --no-pager --full status ultraleap-hand-tracking-service
printf '\n起動: %s/scripts/run.sh\n' "$workspace"
