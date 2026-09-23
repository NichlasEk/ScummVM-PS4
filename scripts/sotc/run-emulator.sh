#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
game_dir=${1:-"$root/build/sotc-DOS/disc-files/SHADOW"}
game_dir=$(CDPATH= cd -- "$game_dir" && pwd)
if [[ ! -f "$game_dir/RES.PAK" || ! -f "$game_dir/D00.PAK" ]]; then
 echo 'Select the original extracted SHADOW directory.' >&2; exit 2
fi
profile_default=xdg-sotc
"$root/scripts/sotc/build.sh"
if [[ ! -x "$root/.tools/shadps4/Shadps4-sdl.AppImage" ]]; then "$root/scripts/setup-emulator.sh"; fi
export XDG_DATA_HOME="$root/.tools/${SOTC_PROFILE:-$profile_default}"
export XDG_CONFIG_HOME="$root/.tools/config-sotc"
export XDG_CACHE_HOME="$root/.tools/cache-sotc"
python3 - "$root" <<'PY'
import json, os, pathlib, shutil, sys
root=pathlib.Path(sys.argv[1]); profile=pathlib.Path(os.environ['XDG_DATA_HOME'])/'shadPS4'
for d in ['data/scummvm/saves','game_data','input_config']:
 (profile/d).mkdir(parents=True,exist_ok=True)
for name in ['config.json','users.json']:
 target=profile/name
 if not target.exists():
  for candidate in ['.tools/xdg-standalone-check/shadPS4','.tools/xdg/shadPS4']:
   source=root/candidate/name
   if source.exists():
    shutil.copyfile(source,target);break
config=profile/'config.json'
if config.exists():
 data=json.loads(config.read_text())
 data.setdefault('Input',{})['use_unified_input_config']=False
 config.write_text(json.dumps(data,indent=2)+'\n')
bindings=profile/'input_config/SCVM00004.ini'
if not bindings.exists():
 text=(root/'config/emulator/SCVM00002.ini').read_text()
 text+='\n# F7 toggles desktop mouse capture.\nmouse_to_joystick = left\nmouse_movement_params = 0.15, 1.0, 0.05\ncross = leftbutton\ncircle = rightbutton\n'
 bindings.write_text(text)
ini=profile/'data/scummvm/scummvm.ini'
if not ini.exists():
 ini.write_text('[scummvm]\nextrapath=/app0/data\ngui_theme=builtin\nsavepath=/data/scummvm/saves\ngfx_mode=normal\njoystick_num=0\nfullscreen=false\nconfirm_exit=false\nps4_usb_sandbox=false\nenable_unsupported_game_warning=false\n')
PY
ulimit -c 0
printf 'SOTC: original game data mounted separately at /mnt/usb0\n'
timeout --kill-after=3s "${EMULATOR_TIMEOUT:-0}s" "$root/.tools/shadps4/Shadps4-sdl.AppImage" --fullscreen false \
 --mount "$game_dir" /mnt/usb0 -- "$root/build/scummvm-sotc-pkg/eboot.bin" > "$root/artifacts/sotc/emulator.log" 2>&1
