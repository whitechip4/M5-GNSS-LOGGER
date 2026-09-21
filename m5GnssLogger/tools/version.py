"""PlatformIO pre-build script: git のリビジョンとビルド日時をマクロとして注入する.

- FIRMWARE_GIT_REV  : 短いコミットハッシュ。未コミット変更があれば "-dirty" を付ける
- FIRMWARE_BUILD_DATE: ビルド日 (YYYY-MM-DD)
セマンティックバージョン (FIRMWARE_VERSION) は include/version.h で手動管理する。
git が使えない環境では "unknown" になる。
"""

import subprocess
from datetime import datetime, timezone

Import("env")  # noqa: F821  (PlatformIO が提供する)


def _git(args):
    try:
        return subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL).decode().strip()
    except Exception:  # noqa: BLE001
        return ""


rev = _git(["rev-parse", "--short=7", "HEAD"]) or "unknown"
if rev != "unknown" and _git(["status", "--porcelain", "--untracked-files=no"]):
    rev += "-dirty"
build_date = datetime.now(timezone.utc).strftime("%Y-%m-%d")

env.Append(
    CPPDEFINES=[
        ("FIRMWARE_GIT_REV", env.StringifyMacro(rev)),
        ("FIRMWARE_BUILD_DATE", env.StringifyMacro(build_date)),
    ]
)
print(f"Firmware build info: git={rev} date={build_date}")
