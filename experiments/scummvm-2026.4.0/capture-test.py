from pathlib import Path
import os, subprocess, time, signal, sys
root=Path(__file__).resolve().parents[2]; mode=sys.argv[1]; assert mode in ('software','vulkan','vulkan-full')
out=root/'artifacts/upstream-2026.4.0'; profile=root/'.tools/upstream-2026.4.0'/('emulator-'+mode)
profile.mkdir(parents=True,exist_ok=True)
data=profile/'data'; (data/'shadPS4').mkdir(parents=True,exist_ok=True)
config=data/'shadPS4/config.json'
if not config.exists(): config.write_bytes((root/'.tools/xdg-scummvm-vulkan/shadPS4/config.json').read_bytes())
stage={'software':'scummvm-2026.4.0-pkg', 'vulkan':'scummvm-2026.4.0-vulkan-pkg', 'vulkan-full':'scummvm-2026.4.0-vulkan-full-pkg'}[mode]
env=dict(os.environ,XDG_DATA_HOME=str(data),XDG_CONFIG_HOME=str(profile/'config'),XDG_CACHE_HOME=str(profile/'cache'),SDL_VIDEODRIVER='x11',SDL_VIDEO_DRIVER='x11')
with (out/(mode+'-emulator.log')).open('w') as log:
 p=subprocess.Popen([str(root/'.tools/shadps4/Shadps4-sdl.AppImage'),'--fullscreen','false',str(root/'build'/stage/'eboot.bin')],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
 try:
  for i in range(30):
   if p.poll() is not None: break
   time.sleep(1)
  subprocess.run(['import','-window','root',str(out/(mode+'-launcher.png'))],check=True)
  print('Emulator return before capture:',p.poll())
 finally:
  if p.poll() is None:
   os.killpg(p.pid,signal.SIGTERM)
   try:p.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
