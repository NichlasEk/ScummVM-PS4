#!/usr/bin/env python3
"""Run under xvfb-run; capture the GPU framebuffer, then verify real pixels."""
import argparse
import ctypes as c
import os
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

parser = argparse.ArgumentParser()
selection = parser.add_mutually_exclusive_group()
selection.add_argument('--uniforms', action='store_true')
selection.add_argument('--textures', action='store_true')
selection.add_argument('--vulkan', action='store_true')
variants = parser.add_mutually_exclusive_group()
variants.add_argument('--resources', action='store_true')
variants.add_argument('--upload', action='store_true')
variants.add_argument('--rectclear', action='store_true')
variants.add_argument('--readback', action='store_true')
variants.add_argument('--sdlgl', action='store_true')
variants.add_argument('--gllegacy', action='store_true')
variants.add_argument('--glsl', action='store_true')
variants.add_argument('--glfbo', action='store_true')
variants.add_argument('--glblend', action='store_true')
variants.add_argument('--gltexture', action='store_true')
variants.add_argument('--glarray', action='store_true')
variants.add_argument('--gluniform', action='store_true')
variants.add_argument('--glindex', action='store_true')
variants.add_argument('--glvbo', action='store_true')
variants.add_argument('--gldraw', action='store_true')
variants.add_argument('--glstate', action='store_true')
variants.add_argument('--twp', action='store_true')
variants.add_argument('--sprites', action='store_true')
variants.add_argument('--composite', action='store_true')
args = parser.parse_args()
if args.sdlgl:
    args.gllegacy = True
if args.gllegacy:
    args.glsl = True
if args.glsl:
    args.glfbo = True
if args.glblend or args.glfbo:
    args.gldraw = True
if args.gltexture:
    args.glindex = True
if args.glarray:
    args.gluniform = True
if args.gluniform:
    args.glindex = True
if args.glindex:
    args.glvbo = True
if args.glvbo:
    args.gldraw = True
if args.glstate:
    args.rectclear = True
if args.rectclear:
    args.readback = True
if args.readback:
    args.composite = True
if args.upload or args.sprites:
    args.resources = True
if (args.resources or args.composite or args.twp or args.gldraw) and not args.vulkan:
    parser.error('Vulkan probe variants require --vulkan')
mode = 'vulkan' if args.vulkan else 'textures' if args.textures else 'uniforms' if args.uniforms else 'triangle'
title_id = 'SCVM00009' if args.vulkan else 'SCVM00008' if args.textures else 'SCVM00007' if args.uniforms else 'SCVM00006'
root = Path(__file__).resolve().parent.parent
os.chdir(root)
profile = root / ('.tools/xdg-vulkan' if args.vulkan else '.tools/xdg-opengnm')
if args.readback:
    # Isolated diagnostic profile: mode 0 disables GPU -> CPU visibility.
    config_path = profile / 'shadPS4/config.json'
    config = json.loads(config_path.read_text()) if config_path.exists() else {}
    config.setdefault('GPU', {})['readbacks_mode'] = 2  # Precise in pinned shadPS4.
    config_path.parent.mkdir(parents=True, exist_ok=True)
    config_path.write_text(json.dumps(config, indent=2))
reports = root / 'artifacts/opengnm'
if mode != 'triangle':
    reports /= mode
if args.vulkan:
    reports = root / 'artifacts/vulkan'
    if args.gldraw:
        reports /= 'sdlgl' if args.sdlgl else 'gllegacy' if args.gllegacy else 'glsl' if args.glsl else 'glfbo' if args.glfbo else 'glblend' if args.glblend else 'gltexture' if args.gltexture else 'glarray' if args.glarray else 'gluniform' if args.gluniform else 'glindex' if args.glindex else 'glvbo' if args.glvbo else 'gldraw'
    elif args.twp:
        reports /= 'twp'
    elif args.composite:
        reports /= 'glstate' if args.glstate else 'rectclear' if args.rectclear else 'readback' if args.readback else 'composite'
    elif args.resources:
        reports /= 'sprites' if args.sprites else 'upload' if args.upload else 'resources'
reports.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, XDG_DATA_HOME=str(profile), SDL_VIDEODRIVER='x11')
emulator = root / '.tools/shadps4/Shadps4-sdl.AppImage'
guest = root / f'.tools/opengnm-stack/freegnm-examples/{mode}/eboot.bin'
guest_log = profile / f'shadPS4/data/scummvm-opengnm-{mode}.log'
if args.vulkan:
    guest = root / '.tools/opengnm-stack/vulkan-ps4/eboot.bin'
    guest_log = profile / 'shadPS4/data/scummvm-vulkan-probe.log'
success_marker = 'PASS Vulkan draw loop' if args.vulkan else 'PASS submission/flip loop'
if guest_log.exists():
    guest_log.unlink()  # This probe's own previous report, never a game save.
screens = profile / 'shadPS4/screenshots'
before = set(screens.glob(title_id + '*.png'))
x = c.CDLL('libX11.so.6')
t = c.CDLL('libXtst.so.6')
x.XOpenDisplay.restype = c.c_void_p
d = x.XOpenDisplay(None)
if not d:
    sys.exit('Run with xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/run-opengnm-probe.py')
x.XDefaultRootWindow.argtypes = [c.c_void_p]
x.XDefaultRootWindow.restype = c.c_ulong
x.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong),
                        c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
x.XSetInputFocus.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_ulong]
x.XFlush.argtypes = [c.c_void_p]
x.XFree.argtypes = [c.c_void_p]
x.XCloseDisplay.argtypes = [c.c_void_p]
x.XKeysymToKeycode.argtypes = [c.c_void_p, c.c_ulong]
x.XKeysymToKeycode.restype = c.c_uint
t.XTestFakeKeyEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]

with (reports / 'triangle-emulator.log').open('w') as log:
    p = subprocess.Popen([str(emulator), '--fullscreen', 'false', str(guest)], env=env, stdout=log, stderr=log)
    try:
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            if guest_log.exists() and 'FRAME 0' in guest_log.read_text():
                break
            if p.poll() is not None:
                raise RuntimeError('Emulator exited before first frame; inspect triangle-emulator.log')
            time.sleep(.25)
        else:
            raise RuntimeError('No first frame; inspect target and emulator logs')
        rr, parent, count = c.c_ulong(), c.c_ulong(), c.c_uint()
        children = c.POINTER(c.c_ulong)()
        x.XQueryTree(d, x.XDefaultRootWindow(d), c.byref(rr), c.byref(parent), c.byref(children), c.byref(count))
        if not count.value:
            raise RuntimeError('No emulator window')
        x.XSetInputFocus(d, children[count.value - 1], 1, 0)
        x.XFree(children)
        key = x.XKeysymToKeycode(d, 0xffc9)  # F12: shadPS4 game-only framebuffer capture.
        t.XTestFakeKeyEvent(d, key, 1, 0)
        x.XFlush(d)
        time.sleep(.1)
        t.XTestFakeKeyEvent(d, key, 0, 0)
        x.XFlush(d)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            candidates = set(screens.glob(title_id + '*.png')) - before
            if candidates and success_marker in guest_log.read_text():
                time.sleep(.3)  # Let the capture writer finish.
                screenshot = max(candidates, key=lambda f: f.stat().st_mtime)
                shutil.copy2(screenshot, reports / 'triangle-gpu.png')
                shutil.copy2(guest_log, reports / 'triangle-result.txt')
                check = subprocess.run([sys.executable, str(root / ('tests/ps4/check-ps4gl-framebuffer.py' if args.glfbo else 'tests/ps4/check-ps4gl-blend.py' if args.glblend else 'tests/ps4/check-ps4gl-vertex.py' if args.glvbo else 'tests/ps4/check-ps4gl-draw.py' if args.gldraw else 'tests/ps4/check-vulkan-twp.py' if args.twp else 'tests/ps4/check-vulkan-composite.py' if args.composite else 'tests/ps4/check-opengnm-textures.py' if args.textures or args.resources else 'tests/ps4/check-opengnm-triangle.py')),
                                        str(reports / 'triangle-gpu.png'), *(['--uniforms'] if args.uniforms else []), *(['--upload'] if args.upload else []), *(['--rectclear'] if args.rectclear else [])],
                                       capture_output=True, text=True)
                (reports / 'pixel-test.log').write_text(check.stdout + check.stderr)
                check.check_returncode()
                print(check.stdout, end='')
                break
            time.sleep(.25)
        else:
            raise RuntimeError('No completed probe/framebuffer capture; F12 binding required')
        if args.vulkan:
            deadline = time.monotonic() + 20
            while 'PASS Vulkan lifecycle' not in guest_log.read_text():
                if time.monotonic() >= deadline or p.poll() is not None:
                    raise RuntimeError('Vulkan cleanup failed; inspect target and emulator logs')
                time.sleep(.25)
            shutil.copy2(guest_log, reports / 'triangle-result.txt')
            if args.sdlgl:
                sdl_log = guest_log.read_text()
                sdl_markers = ('PASS SDL triangle strips arrays uint16 uint32 and retained indices',
                               'PASS SDL numeric framebuffer and shader limits',
                               'PASS SDL empty swap preserves presented image',
                               'PASS SDL teardown discards unfinished frame',
                               'PASS reusable frame owner destruction',
                               'PASS reusable display owner startup',
                               'PASS reusable display owner destruction',
                               'PASS SDL context destruction after quiesce',
                               'PASS SDL context proc lookup',
                               'PASS SDL presentation failure propagation',
                               'PASS SDL failed quiesce retains context for retry')
                if sdl_log.count('PASS SDL swap ') != 8 or any(m not in sdl_log for m in sdl_markers):
                    raise RuntimeError('SDL context/swap lifecycle incomplete')
            if args.gllegacy and guest_log.read_text().count('PASS legacy source and attribute bindings 4 1 6') != 2:
                raise RuntimeError('Legacy shader attribute checks incomplete')
            if args.gllegacy and 'PASS original TWP fade signed integer uniform' not in guest_log.read_text():
                raise RuntimeError('TWP integer fade setup incomplete')
            if args.glsl and (guest_log.read_text().count('GLSL runtime program=') != 2 or
                              guest_log.read_text().count('PASS GL shader objects copied source and deferred deletion') != 2 or
                              'PASS GL shader objects reject cross-stage mismatch' not in guest_log.read_text() or
                              'PASS GL shader objects native interface rejection' not in guest_log.read_text() or
                              guest_log.read_text().count('PASS GLSL runtime rejection ') != 3 or
                              'PASS GLSL runtime compiler finalized' not in guest_log.read_text()):
                raise RuntimeError('Runtime GLSL compiler checks incomplete')
            if args.glfbo and (guest_log.read_text().count('PASS GL framebuffer targets orientation lifetime and rejected feedback') != 8 or
                                guest_log.read_text().count('PASS GL framebuffer storage collected') != 8 or
                                'PASS GL vertex command buffer resubmission' not in guest_log.read_text()):
                raise RuntimeError('GL framebuffer checks incomplete')
            if args.glblend and (guest_log.read_text().count('PASS GL blend variants=8 and state snapshots') != 8 or
                                 'PASS GL vertex command buffer resubmission' not in guest_log.read_text()):
                raise RuntimeError('GL blend state/lifetime checks incomplete')
            if args.gltexture and (guest_log.read_text().count('PASS GL texture units updates filtering and deletion') != 8 or
                                   guest_log.read_text().count('PASS GL RGB8 RGB565 uploads and RGB subimage snapshots') != 8):
                raise RuntimeError('GL texture checks incomplete')
            if args.glarray and guest_log.read_text().count('PASS GL array snapshots and indexed updates') != 8:
                raise RuntimeError('GL array checks incomplete')
            if args.gluniform and guest_log.read_text().count('PASS GL uniform snapshots and names') != 8:
                raise RuntimeError('GL uniform checks incomplete')
            if args.glindex and guest_log.read_text().count('PASS GL index uint16 uint32 offset mutation') != 8:
                raise RuntimeError('GL indexed draw checks incomplete')
            if args.glvbo and (guest_log.read_text().count('PASS GL vertex deleted-buffer rejection') != 8 or
                               'PASS GL vertex command buffer resubmission' not in guest_log.read_text()):
                raise RuntimeError('GL vertex lifetime checks incomplete')
            if args.gldraw and guest_log.read_text().count('PASS GL program rejection ') != 3:
                raise RuntimeError('GL program rejection checks incomplete')
            if args.gldraw and guest_log.read_text().count('GL draw frame ') != 8:
                raise RuntimeError('GL draws did not complete')
            if args.gldraw and guest_log.read_text().count(' error=0') != 8:
                raise RuntimeError('GL draw errors')
            if args.glstate and (guest_log.read_text().count(' error=0') != 8):
                raise RuntimeError('GL state/error checks did not complete')
            if args.readback and (guest_log.read_text().count('PASS readback frame ') != 9 or
                                  'PASS readback command buffer resubmission' not in guest_log.read_text()):
                raise RuntimeError('Readback pixel/guard checks did not complete')
            if args.upload and 'PASS upload command buffer resubmission' not in guest_log.read_text():
                raise RuntimeError('Upload resubmission check did not complete')
            shutil.copy2(guest_log.with_name('scummvm-vulkan-driver.log'), reports / 'driver.log')
            print('PASS Vulkan lifecycle cleanup')
    finally:
        p.terminate()
        try:
            p.wait(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
        x.XCloseDisplay(d)
