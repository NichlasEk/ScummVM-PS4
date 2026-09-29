// PS4 VideoOut has no SDL system cursor. The SDL 3D backend normally uses
// that cursor (it deliberately ignores engine cursor bitmaps), so provide an
// equivalent fixed arrow for GUI overlays. Games retain their own cursor.
void OpenGLSdlGraphics3dManager::drawPS4MenuCursor() {
	if (!_overlayInGUI || !_cursorVisible)
		return;
	if (!_ps4MenuCursor) {
		static const char arrow[18][13] = {
			"X...........", "XX..........", "XOX.........", "XOOX........",
			"XOOOX.......", "XOOOOX......", "XOOOOOX.....", "XOOOOOOX....",
			"XOOOOOOOX...", "XOOOOOOOOX..", "XOOOOXXXXX..", "XOOXOX......",
			"XOX.OX......", "XX..XOX.....", "X...XOX.....", ".....XOX....",
			".....XOX....", "......X....."
		};
		const Graphics::PixelFormat format = OpenGL::TextureGL::getRGBAPixelFormat();
		_ps4MenuCursor = new OpenGL::TiledSurface(12, 18, format);
		Graphics::Surface *pixels = _ps4MenuCursor->getBackingSurface();
		for (int y = 0; y < 18; ++y)
			for (int x = 0; x < 12; ++x) {
				const char c = arrow[y][x];
				const byte value = c == 'O' ? 255 : 0;
				pixels->setPixel(x, y, format.ARGBToColor(c == '.' ? 0 : 255, value, value, value));
			}
		_ps4MenuCursor->update();
	}
	// Cursor coordinates are already in window space, with the tip as hotspot.
	const int scale = MAX(1, _windowHeight / 360);
	const int height = 18 * scale;
	glViewport(_cursorX, _windowHeight - _cursorY - height, 12 * scale, height);
	_ps4MenuCursor->draw(_surfaceRenderer);
}
