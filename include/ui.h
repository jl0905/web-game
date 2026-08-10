#ifndef UI_H
#define UI_H

#include "raylib.h"

// Shared text helpers. The UI font is loaded once by ui_init() (after the
// window exists) and unloaded by ui_destroy(). Plugins draw text through these
// so they don't each own a copy of the font.

void ui_init(void);
void ui_destroy(void);
void ui_text(const char *text, float x, float y, float size, Color c);
float ui_measure(const char *text, float size);

#endif
