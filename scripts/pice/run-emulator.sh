#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
game_dir=${1:-"$root/build/scummvm-dott-pkg/games/pice/cd-eur"}
game_dir=$(CDPATH= cd -- "$game_dir" && pwd)
if [[ ! -f "$game_dir/KSVGA.KRO" || ! -f "$game_dir/E_KLANG.KRO" ]]; then
 echo 'This first preview requires the English European CD directory (KSVGA.KRO and E_KLANG.KRO).' >&2
 exit 2
fi
"$root/scripts/pice/build.sh"
if [[ ! -x "$root/.tools/shadps4/Shadps4-sdl.AppImage" ]]; then "$root/scripts/setup-emulator.sh"; fi
export XDG_DATA_HOME="$root/.tools/xdg-pice"
export XDG_CONFIG_HOME="$root/.tools/config-pice"
export XDG_CACHE_HOME="$root/.tools/cache-pice"
python3 - "$root" <<'PY'
import json, pathlib, shutil, sys
root=pathlib.Path(sys.argv[1]); profile=root/'.tools/xdg-pice/shadPS4'
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
if not bindings.exists(): shutil.copyfile(root/'config/emulator/SCVM00002.ini',bindings)
ini=profile/'data/scummvm/scummvm.ini'
if not ini.exists():
 ini.write_text('[scummvm]\ngui_theme=builtin\nsavepath=/data/scummvm/saves\ngfx_mode=normal\njoystick_num=0\nfullscreen=false\nconfirm_exit=false\nps4_usb_sandbox=false\n')
PY
ulimit -c 0
printf 'PICE resource preview: original game data mounted separately at /mnt/usb0\n'
timeout --kill-after=3s "${EMULATOR_TIMEOUT:-0}s" "$root/.tools/shadps4/Shadps4-sdl.AppImage" --fullscreen false \
 --mount "$game_dir" /mnt/usb0 -- "$root/build/scummvm-pice-pkg/eboot.bin" > "$root/artifacts/pice/emulator.log" 2>&1
