"""Checked source overlays for the isolated PS4 Vulkan runtime."""
import difflib

def runtime_patches(root):
    patches = []
    originals = {}
    modified = {}
    def edit(name, changes):
        p = root / name
        original = originals.setdefault(name, p.read_text())
        text = modified.get(name, original)
        for old, new in changes:
            assert text.count(old) == 1, (name, old, text.count(old))
            text = text.replace(old, new)
        modified[name] = text

    edit('backends/graphics/opengl/texture.cpp', [
        ('\tbind();\n\n\tGL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER', '\t// Preserve the requested filter while context recreation has no texture.\n\tif (!_glTexture)\n\t\treturn;\n\tbind();\n\n\tGL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER')])
    edit('backends/graphics/opengl/opengl-graphics.cpp', [
        ('\t\twarning("Libretro is not supported");\n\t\treturn true;', '\t\tif (!fileName.empty()) {\n\t\t\twarning("Requested shader preset requires Libretro support");\n\t\t\treturn false;\n\t\t}\n\t\treturn true;')])
    edit('backends/fs/posix/posix-fs.cpp', [
        ('#include "common/textconsole.h"', '#include "common/textconsole.h"\n#include "common/debug.h"'),
        ('warning("[ps4-fs] listing %s",', 'debug(5, "[ps4-fs] listing %s",'),
        ('warning("[ps4-fs] listing finished: %s valid=%d entries=%u",', 'debug(5, "[ps4-fs] listing finished: %s valid=%d entries=%u",')])
    edit('backends/mixer/sdl/sdl-mixer.cpp', [
        ('#define SAMPLES_PER_SEC 44100', '#ifdef __PS4__\n#define SAMPLES_PER_SEC 48000\n#else\n#define SAMPLES_PER_SEC 44100\n#endif')])
    edit('engines/twp/actorlib.cpp', [
        ('if (!slot)\n\t\t\t\twarning("slot for actor %s not found", key.c_str());',
         'if (!slot) {\n\t\t\t\t// Scripts set this before registering Franklin in the HUD.\n\t\t\t\tactor->_pendingSelectable = selectable;\n\t\t\t\tactor->_hasPendingSelectable = true;\n\t\t\t}'),
        ('\tg_twp->_hud->_actorSlots[slot - 1].actor = actor;',
         '\tif (slot < 1 || slot > NUMACTORS)\n\t\treturn sq_throwerror(v, "invalid selectable actor slot");\n\tg_twp->_hud->_actorSlots[slot - 1].actor = actor;\n\tif (actor && actor->_hasPendingSelectable) {\n\t\tg_twp->_hud->_actorSlots[slot - 1].selectable = actor->_pendingSelectable;\n\t\tactor->_hasPendingSelectable = false;\n\t}')])
    edit('engines/twp/object.h', [
        ('\tHSQOBJECT _table;', '\tbool _pendingSelectable = false;\n\tbool _hasPendingSelectable = false;\n\tbool _bumperCycle = true;\n\tHSQOBJECT _table;')])
    edit('engines/twp/objlib.cpp', [
        ('\t// TODO: objectBumperCycle\n\twarning("objectBumperCycle not implemented");',
         '\tCommon::SharedPtr<Object> obj = sqobj(v, 2);\n\tbool enabled;\n\tif (!obj || SQ_FAILED(sqget(v, 3, enabled)))\n\t\treturn sq_throwerror(v, "invalid objectBumperCycle arguments");\n\tobj->_bumperCycle = enabled;')])
    edit('engines/twp/twp.cpp', [
        ('if (!g_twp->_room || !g_twp->_inputState._inputActive || !g_twp->_inputState._showCursor)',
         'if (!g_twp->_room || !g_twp->_actor || !g_twp->_inputState._inputActive || !g_twp->_inputState._showCursor)'),
        ('if (g_twp->_resManager->isActor(obj->getId()) || !obj->isTouchable() || !isOnScreen(obj))',
         'if (!obj->_bumperCycle || g_twp->_resManager->isActor(obj->getId()) || !obj->isTouchable() || !isOnScreen(obj))')])
    edit('engines/twp/genlib.cpp', [
        ('\twarning("TODO: markProgress not implemented");',
         '\tCommon::String progress;\n\tif (SQ_FAILED(sqget(v, 2, progress)))\n\t\treturn sq_throwerror(v, "invalid progress");\n\tConfMan.set("twp_progress", progress);\n\tConfMan.flushToDisk();'),
        ('\twarning("TODO: markStat not implemented");',
         '\tCommon::String name;\n\tSQInteger value;\n\tif (SQ_FAILED(sqget(v, 2, name)) || SQ_FAILED(sqget(v, 3, value)))\n\t\treturn sq_throwerror(v, "invalid statistic");\n\tAchMan.setStatInt(name, (int)value);'),
        ('\t// TODO: refreshUI\n\twarning("refreshUI not implemented");',
         '\t// Icon tables and verb preferences are read on every draw. Reset cached\n\t// inventory animation positions when scripts change those tables.\n\tfor (int i = 0; i < NUMACTORS; ++i) {\n\t\tCommon::SharedPtr<Object> actor = g_twp->_hud->_actorSlots[i].actor;\n\t\tif (!actor) continue;\n\t\tfor (auto &obj : actor->_inventory) {\n\t\t\tobj->_iconIndex = 0;\n\t\t\tobj->_iconElapsed = 0.f;\n\t\t}\n\t}')])
    edit('engines/twp/syslib.cpp', [
        ('\twarning("TODO: activeController: not implemented");\n\t// TODO: change harcoded mouse\n\tsq_pushinteger(v, 1);',
         '\t// Both PS4 stick input and the desktop mouse drive the backend cursor.\n\tsq_pushinteger(v, MOUSE);'),
        ('\twarning("TODO: inputController: not implemented");',
         '\tbool enabled;\n\tif (SQ_FAILED(sqget(v, 2, enabled)))\n\t\treturn sq_throwerror(v, "invalid controller input mode");\n\tg_twp->_controllerInput = enabled;'),
        ('\t\twarning("TODO: exCommand EX_OPTIONS_MUSIC: not implemented");',
         '\t\tg_twp->_audio->_soundOptions = sqsounddef(v, 3);\n\t\tif (!g_twp->_audio->_soundOptions && sq_gettype(v, 3) != OT_NULL)\n\t\t\treturn sq_throwerror(v, "invalid options music");')])
    edit('engines/twp/audio.h', [
        ('\tCommon::SharedPtr<SoundDefinition> _soundHover;', '\tCommon::SharedPtr<SoundDefinition> _soundOptions;\n\tCommon::SharedPtr<SoundDefinition> _soundHover;')])
    edit('engines/twp/twp.h', [
        ('\tvoid updateSettingVars();', '\tbool _controllerInput = false;\n\tvoid updateSettingVars();')])
    # Append to the existing twp.cpp diff, not a second diff against old content.
    name = 'engines/twp/twp.cpp'
    # This additional edit is merged below by the caller before git apply.
    edit(name, [
        ('#include "twp/twp.h"', '#include "twp/twp.h"\n#include "engines/dialogs.h"'),
        ('int TwpEngine::runDialog(GUI::Dialog &dialog) {\n\tint result = Engine::runDialog(dialog);',
         'int TwpEngine::runDialog(GUI::Dialog &dialog) {\n\tint result;\n\tif (&dialog == _mainMenuDialog && _audio && _audio->_soundOptions) {\n\t\tPauseToken pause = pauseEngine();\n\t\tint music = _audio->play(_audio->_soundOptions, Audio::Mixer::kMusicSoundType, -1);\n\t\tresult = dialog.runModal();\n\t\t_audio->stop(music);\n\t} else {\n\t\tresult = Engine::runDialog(dialog);\n\t}'),
        ('\t\t\t\tswitch ((TwpAction)e.customType) {',
         '\t\t\t\tif (_controllerInput && _room && sqrawexists(_room->_table, "pressedKey")) {\n\t\t\t\t\tint key = 0;\n\t\t\t\t\tswitch ((TwpAction)e.customType) {\n\t\t\t\t\tcase TwpAction::kSelectVerbInventoryLeft: key = KEY_LEFT; break;\n\t\t\t\t\tcase TwpAction::kSelectVerbInventoryRight: key = KEY_RIGHT; break;\n\t\t\t\t\tcase TwpAction::kSkipCutscene: key = BUTTON_B; break;\n\t\t\t\t\tdefault: break;\n\t\t\t\t\t}\n\t\t\t\t\tif (key) { sqcall(_room->_table, "pressedKey", key); break; }\n\t\t\t\t}\n\t\t\t\tswitch ((TwpAction)e.customType) {')])
    edit('engines/twp/object.h', [
        ('\tbool _bumperCycle = true;', '\tbool _bumperCycle = true;\n\tbool _grassEnabled = false;\n\tbool _grassAnchored = true;\n\tfloat _grassAmplitude = 0.f;\n\tfloat _grassSpeed = 0.f;')])
    edit('engines/twp/objlib.cpp', [
        ('\t// TODO: objectShader\n\twarning("objectShader not implemented");',
         '\tCommon::SharedPtr<Object> obj = sqobj(v, 2);\n\tbool enabled;\n\tif (!obj || SQ_FAILED(sqget(v, 3, enabled)))\n\t\treturn sq_throwerror(v, "invalid objectShader arguments");\n\tif (!enabled) { obj->_grassEnabled = false; return 0; }\n\tSQInteger effect;\n\tfloat amplitude, speed;\n\tbool anchored;\n\tif (SQ_FAILED(sqget(v, 4, effect)) || effect != GRASS_BACKANDFORTH ||\n\t    SQ_FAILED(sqget(v, 5, amplitude)) || SQ_FAILED(sqget(v, 6, speed)) ||\n\t    SQ_FAILED(sqget(v, 7, anchored)))\n\t\treturn sq_throwerror(v, "invalid grass effect parameters");\n\tobj->_grassEnabled = true;\n\tobj->_grassAmplitude = amplitude;\n\tobj->_grassSpeed = speed;\n\tobj->_grassAnchored = anchored;')])
    edit('engines/twp/gfx.h', [
        ('const Math::Matrix4 &trsf = Math::Matrix4(), bool flipX = false, bool flipY = false);\n\tvoid drawSprite(Texture',
         'const Math::Matrix4 &trsf = Math::Matrix4(), bool flipX = false, bool flipY = false, float topShift = 0.f, float bottomShift = 0.f);\n\tvoid drawSprite(Texture')])
    edit('engines/twp/gfx.cpp', [
        ('\tglReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());', '\tGL_CALL(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data()));'),
        ('void Gfx::drawSprite(const Common::Rect &textRect, Texture &texture, const Color &color, const Math::Matrix4 &trsf, bool flipX, bool flipY) {',
         'void Gfx::drawSprite(const Common::Rect &textRect, Texture &texture, const Color &color, const Math::Matrix4 &trsf, bool flipX, bool flipY, float topShift, float bottomShift) {'),
        ('\t\t{{pos.getX(), pos.getY() + textRect.height()}, color, {l, t}}};\n\tuint32 quadIndices[] = {', '\t\t{{pos.getX(), pos.getY() + textRect.height()}, color, {l, t}}};\n\t// Deform geometry before the existing lighting shader; keep atlas UVs intact.\n\tvertices[0].pos.setX(vertices[0].pos.getX() + topShift);\n\tvertices[3].pos.setX(vertices[3].pos.getX() + topShift);\n\tvertices[1].pos.setX(vertices[1].pos.getX() + bottomShift);\n\tvertices[2].pos.setX(vertices[2].pos.getX() + bottomShift);\n\tuint32 quadIndices[] = {')])
    edit('engines/twp/scenegraph.cpp', [
        ('void Anim::drawCore(const Math::Matrix4 &t) {\n\tMath::Matrix4 trsf(t);',
         'void Anim::drawCore(const Math::Matrix4 &t) {\n\tMath::Matrix4 trsf(t);\n\tconst float sway = _obj->_grassEnabled ? _obj->_grassAmplitude * sinf(g_twp->_time * _obj->_grassSpeed) : 0.f;\n\tconst float baseSway = _obj->_grassAnchored ? 0.f : sway;'),
        ('g_twp->getGfx().drawSprite(Common::Rect(texture->width, texture->height), *texture, getComputedColor(), trsf, flipX);',
         'g_twp->getGfx().drawSprite(Common::Rect(texture->width, texture->height), *texture, getComputedColor(), trsf, flipX, false, sway, baseSway);'),
        ('g_twp->getGfx().drawSprite(sf->frame, *texture, getComputedColor(), trsf, flipX);',
         'g_twp->getGfx().drawSprite(sf->frame, *texture, getComputedColor(), trsf, flipX, false, sway, baseSway);')])
    edit('engines/twp/audio.cpp', [
        ('int id = g_twp->_resManager->newSoundId();', 'const int duration = audioStream->getLength().msecs();\n\tint id = g_twp->_resManager->newSoundId();'),
        ('slot->total = audioStream->getLength().msecs();', 'slot->total = duration;'),
        ('g_twp->_mixer->playStream(cat, &slot->handle, audioStream, id, volume * _masterVolume);',
         '// Let the mixer loop infinite music even while the engine is paused in a dialog.\n\tg_twp->_mixer->playStream(cat, &slot->handle, loopTimes == -1 ? Audio::makeLoopingAudioStream(audioStream, 0) : audioStream, id, volume * _masterVolume);')])
    edit('engines/twp/savegame.cpp', [
        ('\tsqcall("postSave");', '\tif (sqrawexists(sqrootTbl(g_twp->getVm()), "postSave"))\n\t\tsqcall("postSave");')])
    edit('engines/twp/gfx.h', [
        ('\tvoid load(const Graphics::Surface &surface);', '\tvoid ensureResident() const;\n\tCommon::String _resourceName;\n\tuint64 _lastUse = 0;\n\tvoid load(const Graphics::Surface &surface);')])
    edit('engines/twp/gfx.cpp', [
        ('#include "twp/gfx.h"', '#include "twp/gfx.h"\n#include "twp/resmanager.h"'),
        ('void Texture::bind(const Texture *texture) {', 'void Texture::ensureResident() const {\n\tif (!_resourceName.empty())\n\t\tg_twp->_resManager->texture(_resourceName);\n}\n\nvoid Texture::bind(const Texture *texture) {\n\tif (texture) texture->ensureResident();'),
        ('\n\t\t_texture = texture ? texture : &_emptyTexture;\n', '\n\t\t_texture = texture ? texture : &_emptyTexture;\n\t\t_texture->ensureResident();\n'),
        ('\t\t\t_texture = texture ? texture : &_emptyTexture;\n', '\t\t\t_texture = texture ? texture : &_emptyTexture;\n\t\t\t_texture->ensureResident();\n')])
    edit('engines/twp/resmanager.h', [
        ('\tstatic Common::String getKey', '\t~ResManager();\n\tstatic Common::String getKey'),
        ('\tint _roomId = START_ROOMID;', '\tuint64 _textureClock = 0;\n\tuint64 _textureBytes = 0;\n\tint _roomId = START_ROOMID;')])
    edit('engines/twp/resmanager.cpp', [
        ('Common::String ResManager::getKey', 'ResManager::~ResManager() {\n\tfor (auto &entry : _textures) {\n\t\tif (entry._value.id) glDeleteTextures(1, &entry._value.id);\n\t}\n}\n\nCommon::String ResManager::getKey'),
        ('\t_textures[name].load(*surface);', '''\t// Bound decoded GPU texture residency while retaining stable Texture objects.
\t// Evicted PNGs are reloaded from the pack on the next bind, including fonts.
\tconst uint64 bytes = (uint64)surface->w * surface->h * 4;
\tconst uint64 budget = 32 * 1024 * 1024;
\tif (bytes > budget) error("Texture %s exceeds resident cache budget", name.c_str());
\twhile (_textureBytes + bytes > budget) {
\t\tTexture *oldest = nullptr;
\t\tfor (auto &entry : _textures) {
\t\t\tTexture &candidate = entry._value;
\t\t\tif (entry._key != name && candidate.id && (!oldest || candidate._lastUse < oldest->_lastUse))
\t\t\t\toldest = &candidate;
\t\t}
\t\tif (!oldest) error("Texture cache accounting mismatch");
\t\tdebugC(kDebugRes, "Evict texture %s", oldest->_resourceName.c_str());
\t\tglDeleteTextures(1, &oldest->id);
\t\toldest->id = 0;
\t\t_textureBytes -= (uint64)oldest->width * oldest->height * 4;
\t}
\tTexture &texture = _textures[name];
\ttexture._resourceName = name;
\ttexture.load(*surface);
\t_textureBytes += bytes;'''),
        ('if (!_textures.contains(key)) {', 'if (!_textures.contains(key) || !_textures[key].id) {'),
        ('\treturn &_textures[key];', '\t_textures[key]._lastUse = ++_textureClock;\n\treturn &_textures[key];')])
    edit('gui/themes/scummtheme.py', [
        ('head = re.compile("<\\?(.*?)\\?>")', 'head = re.compile(r"<\\?(.*?)\\?>")')])
    edit('configure', [
        ('\tcase "$_host" in\n\t\t3ds)', '\tcase "$_host" in\n\t\tx86_64-pc-freebsd)\n\t\t\t# OpenOrbis cross flags and dependency paths are supplied by our wrapper.\n\t\t\t_backend="sdl"\n\t\t\t;;\n\t\t3ds)')])
    # Fix compiler diagnostics at their source; keep warning flags enabled.
    for name in ('audio', 'motor', 'util'):
        path = 'engines/twp/' + name + '.cpp'
        current = modified.get(path, (root / path).read_text())
        changes = [(line, line.replace('abs(', 'fabsf(')) for line in current.splitlines(True) if 'abs(' in line]
        edit(path, changes)
    edit('engines/twp/genlib.cpp', [
        ('float d = abs(num1 - num2);', 'float d = fabs((double)num1 - (double)num2);')])
    edit('engines/twp/squirrel/squtils.h', [
        ('        _vals[idx].~T();\n        if(idx < (_size - 1)) {\n            memmove(&_vals[idx], &_vals[idx+1], sizeof(T) * (_size - idx - 1));\n        }\n        _size--;',
         '        // Assignment preserves reference counts for nontrivial values.\n        for(SQUnsignedInteger i = idx; i + 1 < _size; ++i)\n            _vals[i] = _vals[i + 1];\n        pop_back();')])
    edit('backends/vkeybd/virtual-keyboard-parser.h', [
        ('virtual void cleanup();', 'void cleanup() override;'),
        ('virtual bool closedKeyCallback(ParserNode *node);', 'bool closedKeyCallback(ParserNode *node) override;')])
    edit('backends/graphics/surfacesdl/surfacesdl-graphics.cpp', [
        ('\tint rotangle = 0;\n', ''),
        ('\trotangle = rotation;\n', ''),
        ('if (rotangle != 0)', 'if (rotation != 0)'),
        ('&viewport, rotangle, nullptr, SDL_FLIP_NONE)', '&viewport, rotation, nullptr, SDL_FLIP_NONE)')])
    edit('audio/softsynth/fmtowns_pc98/towns_pc98_fmsynth.cpp', [
        ('TownsPC98_FmSynth::ChanInternal::ChanInternal() {\n\tmemset(this, 0, sizeof(ChanInternal));\n}',
         'TownsPC98_FmSynth::ChanInternal::ChanInternal()\n\t: enableLeft(false), enableRight(false), updateEnvelopeParameters(false),\n\t  feedbuf{}, algorithm(0), ampModSvty(0), frqModSvty(0), opr{} {\n}')])
    # Based on upstream 5f91b1bbd2123c7dd3fa11c8153fa617e4c1fea0: D5-D10
    # share CASt headers. Keep the newer parser gate opt-in for this D7 trial.
    edit('engines/director/cast.cpp', [
        ('} else if (_version >= kFileVer500 && _version < kFileVer600) {\n\t\tcastType = stream.readUint32();',
         '} else if (_version >= kFileVer500 && (_version < kFileVer600 ||\n\t\t(_version < kFileVer800 && Common::String(_vm->getGameId()) == "jonssonligan1" &&\n\t\t ConfMan.hasKey("ps4_director7_experimental") && ConfMan.getBool("ps4_director7_experimental")))) {\n\t\tcastType = stream.readUint32();')])
    # Watchmaker keeps its existing geometry code; the experimental bridge
    # lowers its bounded immediate-mode subset to shaders. Do not enable the
    # general classic-GL renderer selection in other engines.
    edit('engines/watchmaker/configure.engine', [
        ('opengl_game_classic', 'opengl_game_shaders')])
    for name in ('renderer.cpp', 'render.cpp', '3d/render/opengl_texture.cpp',
                 '3d/render/opengl_renderer.cpp', '3d/render/opengl_3d.cpp', '3d/render/opengl_2d.cpp'):
        path = 'engines/watchmaker/' + name
        original = (root / path).read_text()
        # The guard also occurs in end-of-block comments.
        originals[path] = original
        modified[path] = original.replace('USE_OPENGL_GAME', 'USE_OPENGL_SHADERS')
        if name in ('3d/render/opengl_texture.cpp', '3d/render/opengl_renderer.cpp'):
            modified[path] = modified[path].replace('GL_CLAMP)', 'GL_CLAMP_TO_EDGE)').replace('GL_CLAMP;', 'GL_CLAMP_TO_EDGE;')
    for name, text in modified.items():
        patches.extend(difflib.unified_diff(originals[name].splitlines(True), text.splitlines(True), fromfile='a/'+name, tofile='b/'+name))
    from pathlib import Path
    patches.extend((Path(__file__).resolve().parents[1] / "patches/scummvm/director7-experimental.patch").read_text().splitlines(True))
    patches.extend((Path(__file__).resolve().parents[1] / "patches/scummvm/watchmaker-experimental.patch").read_text().splitlines(True))
    patches.extend((Path(__file__).resolve().parents[1] / "patches/scummvm/hpl1-experimental.patch").read_text().splitlines(True))
    return patches
