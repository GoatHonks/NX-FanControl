#include <fancontrol.hpp>

#include "ui.hpp"

#include <SDL2/SDL2_gfxPrimitives.h>

#include <algorithm>
#include <unordered_map>

namespace ui {

    namespace {

        SDL_Renderer *g_renderer = nullptr;
        TTF_Font     *g_fonts[FontCount] = {};
        PlFontData    g_fontData{};
        bool          g_plReady = false;

        /* Rendered strings are cached because the same labels are drawn every
         * frame; re-rasterising them at 60fps is pure waste. The set of
         * distinct strings in this app is small and bounded. */
        std::unordered_map<std::string, SDL_Texture *> g_textCache;

        constexpr int FontPx[FontCount] = { 14, 17, 21, 26, 32, 54 };

        SDL_Texture *GetText(const std::string &text, Font font, SDL_Color c) {
            if (text.empty() || g_fonts[font] == nullptr) {
                return nullptr;
            }

            char key[64];
            snprintf(key, sizeof(key), "%d|%02x%02x%02x%02x|", (int)font, c.r, c.g, c.b, c.a);
            std::string cacheKey = std::string(key) + text;

            auto it = g_textCache.find(cacheKey);
            if (it != g_textCache.end()) {
                return it->second;
            }

            SDL_Surface *surface = TTF_RenderUTF8_Blended(g_fonts[font], text.c_str(), c);
            if (surface == nullptr) {
                return nullptr;
            }
            SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surface);
            SDL_FreeSurface(surface);

            g_textCache[cacheKey] = tex;
            return tex;
        }

    }

    bool Init(SDL_Renderer *renderer) {
        g_renderer = renderer;

        if (TTF_Init() != 0) {
            return false;
        }

        /* Use the console's own shared font so the app matches the system UI
         * instead of shipping a lookalike. */
        if (R_FAILED(plInitialize(PlServiceType_User))) {
            return false;
        }
        g_plReady = true;

        if (R_FAILED(plGetSharedFontByType(&g_fontData, PlSharedFontType_Standard))) {
            return false;
        }

        for (int i = 0; i < FontCount; ++i) {
            SDL_RWops *rw = SDL_RWFromConstMem(g_fontData.address, g_fontData.size);
            if (rw == nullptr) {
                return false;
            }
            g_fonts[i] = TTF_OpenFontRW(rw, 1, FontPx[i]);
            if (g_fonts[i] == nullptr) {
                return false;
            }
        }

        return true;
    }

    void Exit(void) {
        for (auto &entry : g_textCache) {
            if (entry.second != nullptr) {
                SDL_DestroyTexture(entry.second);
            }
        }
        g_textCache.clear();

        for (int i = 0; i < FontCount; ++i) {
            if (g_fonts[i] != nullptr) {
                TTF_CloseFont(g_fonts[i]);
                g_fonts[i] = nullptr;
            }
        }

        if (g_plReady) {
            plExit();
            g_plReady = false;
        }
        TTF_Quit();
    }

    void Clear(SDL_Color c) {
        SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
        SDL_RenderClear(g_renderer);
    }

    void Present(void) {
        SDL_RenderPresent(g_renderer);
    }

    void Rect(int x, int y, int w, int h, SDL_Color c) {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
        SDL_Rect r = { x, y, w, h };
        SDL_RenderFillRect(g_renderer, &r);
    }

    void RoundRect(int x, int y, int w, int h, int radius, SDL_Color c) {
        roundedBoxRGBA(g_renderer, x, y, x + w - 1, y + h - 1, radius, c.r, c.g, c.b, c.a);
    }

    void RoundRectOutline(int x, int y, int w, int h, int radius, SDL_Color c) {
        roundedRectangleRGBA(g_renderer, x, y, x + w - 1, y + h - 1, radius, c.r, c.g, c.b, c.a);
    }

    void Line(int x1, int y1, int x2, int y2, SDL_Color c) {
        aalineRGBA(g_renderer, x1, y1, x2, y2, c.r, c.g, c.b, c.a);
    }

    void ThickLine(int x1, int y1, int x2, int y2, int width, SDL_Color c) {
        thickLineRGBA(g_renderer, x1, y1, x2, y2, width, c.r, c.g, c.b, c.a);
    }

    void Circle(int x, int y, int radius, SDL_Color c) {
        filledCircleRGBA(g_renderer, x, y, radius, c.r, c.g, c.b, c.a);
        aacircleRGBA(g_renderer, x, y, radius, c.r, c.g, c.b, c.a);
    }

    void CircleOutline(int x, int y, int radius, SDL_Color c) {
        aacircleRGBA(g_renderer, x, y, radius, c.r, c.g, c.b, c.a);
    }

    void Text_(const std::string &text, int x, int y, Font font, SDL_Color c) {
        SDL_Texture *tex = GetText(text, font, c);
        if (tex == nullptr) {
            return;
        }
        int w = 0, h = 0;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        SDL_Rect dst = { x, y, w, h };
        SDL_RenderCopy(g_renderer, tex, nullptr, &dst);
    }

    void TextCentered(const std::string &text, int cx, int y, Font font, SDL_Color c) {
        Text_(text, cx - TextWidth(text, font) / 2, y, font, c);
    }

    void TextRight(const std::string &text, int rightX, int y, Font font, SDL_Color c) {
        Text_(text, rightX - TextWidth(text, font), y, font, c);
    }

    int TextWidth(const std::string &text, Font font) {
        if (text.empty() || g_fonts[font] == nullptr) {
            return 0;
        }
        int w = 0, h = 0;
        TTF_SizeUTF8(g_fonts[font], text.c_str(), &w, &h);
        return w;
    }

    int TextHeight(Font font) {
        return g_fonts[font] != nullptr ? TTF_FontHeight(g_fonts[font]) : 0;
    }

    /* ---- fan curve plot ---- */

    namespace {
        constexpr int GraphMinTemp = 20;
        constexpr int GraphMaxTemp = 80;

        int TempToX(int plotX, int plotW, float tempC) {
            const float t = (tempC - GraphMinTemp) / (float)(GraphMaxTemp - GraphMinTemp);
            return plotX + (int)(std::clamp(t, 0.0f, 1.0f) * plotW);
        }

        int LevelToY(int plotY, int plotH, float level) {
            return plotY + plotH - (int)(std::clamp(level, 0.0f, 1.0f) * plotH);
        }
    }

    void CurveGraph(int x, int y, int w, int h,
                    const TemperaturePoint *points, u32 count,
                    float liveTemp, int selectedPoint) {

        constexpr int PadLeft   = 52;
        constexpr int PadBottom = 34;
        constexpr int PadTop    = 14;
        constexpr int PadRight  = 16;

        const int plotX = x + PadLeft;
        const int plotY = y + PadTop;
        const int plotW = w - PadLeft - PadRight;
        const int plotH = h - PadTop - PadBottom;

        /* horizontal gridlines every 25% */
        for (int pct = 0; pct <= 100; pct += 25) {
            const int gy = LevelToY(plotY, plotH, pct / 100.0f);
            Line(plotX, gy, plotX + plotW, gy, Border);
            TextRight(std::to_string(pct) + "%", plotX - 10, gy - TextHeight(FontSmall) / 2, FontSmall, TextFaint);
        }

        /* vertical gridlines every 10C */
        for (int t = GraphMinTemp; t <= GraphMaxTemp; t += 10) {
            const int gx = TempToX(plotX, plotW, (float)t);
            Line(gx, plotY, gx, plotY + plotH, Border);
            TextCentered(std::to_string(t), gx, plotY + plotH + 8, FontSmall, TextFaint);
        }

        if (points == nullptr || count == 0) {
            return;
        }

        /* Shaded area under the curve, drawn as vertical spans so it follows
         * the interpolation the sysmodule actually uses. */
        for (int px = 0; px < plotW; ++px) {
            const float tempC = GraphMinTemp + (px / (float)plotW) * (GraphMaxTemp - GraphMinTemp);
            const float level = std::clamp(InterpolateFanLevel(points, count, tempC), 0.0f, 1.0f);
            const int   ly    = LevelToY(plotY, plotH, level);
            Rect(plotX + px, ly, 1, plotY + plotH - ly, SDL_Color{ Accent.r, Accent.g, Accent.b, 38 });
        }

        /* Curve line */
        for (u32 i = 0; i + 1 < count; ++i) {
            const int x1 = TempToX(plotX, plotW, (float)points[i].temperature_c);
            const int y1 = LevelToY(plotY, plotH, points[i].fanLevel_f);
            const int x2 = TempToX(plotX, plotW, (float)points[i + 1].temperature_c);
            const int y2 = LevelToY(plotY, plotH, points[i + 1].fanLevel_f);
            ThickLine(x1, y1, x2, y2, 3, Accent);
        }

        /* Flat runs before the first and after the last point, matching how
         * InterpolateFanLevel clamps outside the table. */
        if (count > 0) {
            const int firstX = TempToX(plotX, plotW, (float)points[0].temperature_c);
            const int firstY = LevelToY(plotY, plotH, points[0].fanLevel_f);
            if (firstX > plotX) {
                ThickLine(plotX, firstY, firstX, firstY, 3, Accent);
                Line(plotX, firstY, firstX, firstY, Accent);
            }
            const int lastX = TempToX(plotX, plotW, (float)points[count - 1].temperature_c);
            const int lastY = LevelToY(plotY, plotH, points[count - 1].fanLevel_f);
            if (lastX < plotX + plotW) {
                ThickLine(lastX, lastY, plotX + plotW, lastY, 3, Accent);
            }
        }

        /* Live temperature marker */
        if (liveTemp >= 0.0f) {
            const int lx = TempToX(plotX, plotW, liveTemp);
            for (int gy = plotY; gy < plotY + plotH; gy += 6) {
                Line(lx, gy, lx, std::min(gy + 3, plotY + plotH), Warn);
            }
            const float level = std::clamp(InterpolateFanLevel(points, count, liveTemp), 0.0f, 1.0f);
            Circle(lx, LevelToY(plotY, plotH, level), 5, Warn);
        }

        /* Points on top */
        for (u32 i = 0; i < count; ++i) {
            const int px = TempToX(plotX, plotW, (float)points[i].temperature_c);
            const int py = LevelToY(plotY, plotH, points[i].fanLevel_f);
            const bool sel = ((int)i == selectedPoint);

            if (sel) {
                Circle(px, py, 9, Text);
                Circle(px, py, 6, Accent);
            } else {
                Circle(px, py, 5, Surface);
                CircleOutline(px, py, 5, Accent);
                Circle(px, py, 3, Accent);
            }
        }
    }

}
