#include "ui.h"

#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

static Font uiFont;
static bool uiOwnsFont;

void ui_init(void)
{
    // Rasterized large and drawn smaller so it stays crisp after window scaling
    uiFont = LoadFontEx("assets/MedievalSharp.ttf", 64, NULL, 0);
    uiOwnsFont = uiFont.texture.id != GetFontDefault().texture.id;
    if (!uiOwnsFont) uiFont = GetFontDefault();
    SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
}

void ui_destroy(void)
{
    if (uiOwnsFont) UnloadFont(uiFont);
}

void ui_text(const char *text, float x, float y, float size, Color c)
{
    DrawTextEx(uiFont, text, (Vector2){ x, y }, size, 1.0f, c);
}

float ui_measure(const char *text, float size)
{
    return MeasureTextEx(uiFont, text, size, 1.0f).x;
}
