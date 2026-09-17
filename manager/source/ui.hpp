#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>

namespace ui {

    constexpr int ScreenW = 1280;
    constexpr int ScreenH = 720;

    /* A restrained dark palette: two surface levels, one accent, and semantic
     * colours used sparingly. */
    constexpr SDL_Color Bg         = {  16,  18,  21, 255 };
    constexpr SDL_Color Surface    = {  24,  27,  32, 255 };
    constexpr SDL_Color SurfaceAlt = {  32,  36,  43, 255 };
    constexpr SDL_Color Border     = {  42,  47,  55, 255 };
    constexpr SDL_Color Text       = { 236, 239, 244, 255 };
    constexpr SDL_Color TextDim    = { 139, 147, 159, 255 };
    constexpr SDL_Color TextFaint  = {  90,  97, 107, 255 };
    constexpr SDL_Color Accent     = {  88, 166, 255, 255 };
    constexpr SDL_Color AccentSoft = {  31,  58,  95, 255 };
    constexpr SDL_Color Good       = {  63, 185,  80, 255 };
    constexpr SDL_Color Warn       = { 210, 153,  34, 255 };
    constexpr SDL_Color Danger     = { 248,  81,  73, 255 };

    enum Font {
        FontTiny = 0,
        FontSmall,
        FontBody,
        FontHead,
        FontTitle,
        FontHuge,
        FontCount,
    };

    bool Init(SDL_Renderer *renderer);
    void Exit(void);

    void Clear(SDL_Color c);
    void Present(void);

    void Rect(int x, int y, int w, int h, SDL_Color c);
    void RoundRect(int x, int y, int w, int h, int radius, SDL_Color c);
    void RoundRectOutline(int x, int y, int w, int h, int radius, SDL_Color c);
    void Line(int x1, int y1, int x2, int y2, SDL_Color c);
    void ThickLine(int x1, int y1, int x2, int y2, int width, SDL_Color c);
    void Circle(int x, int y, int radius, SDL_Color c);
    void CircleOutline(int x, int y, int radius, SDL_Color c);

    void Text_(const std::string &text, int x, int y, Font font, SDL_Color c);
    void TextCentered(const std::string &text, int cx, int y, Font font, SDL_Color c);
    void TextRight(const std::string &text, int rightX, int y, Font font, SDL_Color c);
    int  TextWidth(const std::string &text, Font font);
    int  TextHeight(Font font);

    /* Draws the fan curve plot. selectedPoint < 0 means none highlighted;
     * liveTemp < 0 hides the live temperature marker. */
    void CurveGraph(int x, int y, int w, int h,
                    const TemperaturePoint *points, u32 count,
                    float liveTemp, int selectedPoint);

}
