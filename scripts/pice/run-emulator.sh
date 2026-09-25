#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
game_dir=${1:-"$root/build/scummvm-dott-pkg/games/pice/cd-eur"}
game_dir=$(CDPATH= cd -- "$game_dir" && pwd)
resource_dir="$game_dir"
if [[ -f "$game_dir/SLPS_011.29" && -f "$game_dir/DATA/KVGA.KRO" ]]; then
 resource_dir="$game_dir/DATA"
elif [[ -f "$game_dir/MAC_KRO/KSVGA.KRO" ]]; then
 resource_dir="$game_dir/MAC_KRO"
fi
if [[ ! -f "$resource_dir/KSVGA.KRO" && ! -f "$resource_dir/KVGA.KRO" ]] || ! compgen -G "$resource_dir/*_KLANG.KRO" >/dev/null; then
 echo 'Expected original PICE graphics and language archives (PS1: select the disc root).' >&2
 exit 2
fi
profile_default=xdg-pice
if [[ -f "$game_dir/SLPS_011.29" ]]; then profile_default=xdg-pice-ps1; fi
"$root/scripts/pice/build.sh"
if [[ ! -x "$root/.tools/shadps4/Shadps4-sdl.AppImage" ]]; then "$root/scripts/setup-emulator.sh"; fi
export XDG_DATA_HOME="$root/.tools/${PICE_PROFILE:-$profile_default}"
export XDG_CONFIG_HOME="$root/.tools/config-pice"
export XDG_CACHE_HOME="$root/.tools/cache-pice"
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
bindings=profile/'input_config/SCVM00003.ini'
if not bindings.exists():
 text=(root/'config/emulator/SCVM00002.ini').read_text()
 text+='\n# F7 toggles desktop mouse capture.\nmouse_to_joystick = left\nmouse_movement_params = 0.15, 1.0, 0.05\ncross = leftbutton\ncircle = rightbutton\n'
 bindings.write_text(text)
ini=profile/'data/scummvm/scummvm.ini'
if not ini.exists():
 ini.write_text('[scummvm]\nextrapath=/app0/data\ngui_theme=scummremastered\nsavepath=/data/scummvm/saves\ngfx_mode=normal\njoystick_num=0\nfullscreen=false\nconfirm_exit=false\nps4_usb_sandbox=false\nenable_unsupported_game_warning=false\n')
PY
ulimit -c 0
printf 'PICE: original game data mounted separately at /mnt/usb0\n'
timeout --kill-after=3s "${EMULATOR_TIMEOUT:-0}s" "$root/.tools/shadps4/Shadps4-sdl.AppImage" --fullscreen false \
 --mount "$game_dir" /mnt/usb0 -- "$root/build/scummvm-pice-pkg/eboot.bin" > "$root/artifacts/pice/emulator.log" 2>&1
