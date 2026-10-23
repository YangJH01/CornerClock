// Digital face: one baseline, fixed digit cells and quiet separators.
// Formats are cached; the clock still redraws only at its existing tick rate.
#include "faces.h"

struct Typography {
    const WCHAR* family;
    float size, cell, colon;
    DWRITE_FONT_WEIGHT weight;
};
static const Typography STYLES[] = {
    { L"Segoe UI", 26, 14.5f, 7, DWRITE_FONT_WEIGHT_MEDIUM },
    { L"Segoe UI", 32, 18.5f, 8, DWRITE_FONT_WEIGHT_SEMI_BOLD },
    { L"Consolas", 28, 16.8f, 11, DWRITE_FONT_WEIGHT_NORMAL },
};
static const UINT STYLE_NAMES[] = {
    IDS_CHOICE_CLEAN, IDS_CHOICE_BOLD_TYPE, IDS_CHOICE_MONO, 0
};
static_assert(_countof(STYLE_NAMES) == _countof(STYLES) + 1);

static const float DATE_W = 64, SIDE_W = 28;
static const float BY0 = 99, BY1 = 141, BMID = 120;
static int o_style = 0;
static int o_hour24 = 1, o_ampm = 0, o_date = 0, o_panel = 0;
static int o_text = RGB(255, 255, 255), o_muted = RGB(169, 180, 196);
static const FaceOpt s_opts[] = {
    { L"Typography", IDS_OPT_TYPOGRAPHY, OPT_CHOICE, &o_style, STYLE_NAMES },
    { L"24-hour clock", IDS_OPT_HOUR24, OPT_BOOL, &o_hour24, nullptr },
    { L"Show AM/PM", IDS_OPT_SHOW_AMPM, OPT_BOOL, &o_ampm, nullptr },
    { L"Show date", IDS_OPT_SHOW_DATE, OPT_BOOL, &o_date, nullptr },
    { L"Show panel", IDS_OPT_SHOW_PANEL, OPT_BOOL, &o_panel, nullptr },
    { L"Text colour", IDS_OPT_TEXT_COLOUR, OPT_COLOR, &o_text, nullptr },
    { L"Label colour", IDS_OPT_LABEL_COLOUR, OPT_COLOR, &o_muted, nullptr },
};

struct Bar { float x0, x1, time, side; };
static Bar LayOut()
{
    const Typography& s = STYLES[o_style];
    float run = (ShowSeconds() ? 6 : 4) * s.cell + (ShowSeconds() ? 2 : 1) * s.colon;
    float width = 20 + run + (o_date ? DATE_W : 0) + (o_ampm ? SIDE_W : 0);
    Bar r;
    r.x0 = CX - width / 2;
    r.x1 = CX + width / 2;
    r.time = r.x0 + 10 + (o_date ? DATE_W : 0);
    r.side = r.time + run;
    return r;
}

D2D1_RECT_F DigitalBounds()
{
    const Bar r = LayOut();
    return D2D1::RectF(r.x0, BY0, r.x1, BY1);
}

class DigitalFace : public IClockFace {
    struct Glyphs {
        ID2D1PathGeometry* path[11] = {};
        float advance[11] = {};
        float baseline = 0;
        bool attempted = false, ready = false;
        ~Glyphs() { for (auto& p : path) SafeRelease(p); }
    };
    mutable Glyphs m_glyphs[_countof(STYLES)];
    mutable IDWriteTextFormat* m_time[_countof(STYLES)] = {};
    mutable IDWriteTextFormat* m_day = nullptr;
    mutable IDWriteTextFormat* m_label = nullptr;

    void CacheGlyphs(ID2D1RenderTarget* rt) const
    {
        Glyphs& cache = m_glyphs[o_style];
        if (cache.attempted) return;
        cache.attempted = true;
        const Typography& style = STYLES[o_style];
        IDWriteFontCollection* fonts = nullptr;
        IDWriteFontFamily* family = nullptr;
        IDWriteFont* font = nullptr;
        IDWriteFontFace* face = nullptr;
        ID2D1Factory* factory = nullptr;
        IDWriteFactory* dw = SharedDWrite();
        UINT32 index = 0;
        BOOL found = FALSE;
        HRESULT hr = dw ? dw->GetSystemFontCollection(&fonts) : E_FAIL;
        if (SUCCEEDED(hr)) hr = fonts->FindFamilyName(style.family, &index, &found);
        if (SUCCEEDED(hr) && !found) hr = E_FAIL;
        if (SUCCEEDED(hr)) hr = fonts->GetFontFamily(index, &family);
        if (SUCCEEDED(hr)) hr = family->GetFirstMatchingFont(style.weight,
            DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
        if (SUCCEEDED(hr)) hr = font->CreateFontFace(&face);
        if (SUCCEEDED(hr)) {
            rt->GetFactory(&factory);
            DWRITE_FONT_METRICS metrics;
            face->GetMetrics(&metrics);
            float scale = style.size / metrics.designUnitsPerEm;
            cache.baseline = BMID + (metrics.ascent - metrics.descent) * scale / 2;
            for (int i = 0; i < 11 && SUCCEEDED(hr); i++) {
                UINT32 codepoint = i < 10 ? L'0' + i : LocTimeSep()[0];
                UINT16 glyph = 0;
                DWRITE_GLYPH_METRICS gm = {};
                hr = face->GetGlyphIndices(&codepoint, 1, &glyph);
                if (SUCCEEDED(hr)) hr = face->GetDesignGlyphMetrics(&glyph, 1, &gm);
                cache.advance[i] = gm.advanceWidth * scale;
                if (SUCCEEDED(hr)) hr = factory->CreatePathGeometry(&cache.path[i]);
                ID2D1GeometrySink* sink = nullptr;
                if (SUCCEEDED(hr)) hr = cache.path[i]->Open(&sink);
                if (SUCCEEDED(hr)) hr = face->GetGlyphRunOutline(style.size, &glyph,
                    nullptr, nullptr, 1, FALSE, FALSE, sink);
                if (sink) {
                    HRESULT closed = sink->Close();
                    if (SUCCEEDED(hr)) hr = closed;
                    sink->Release();
                }
            }
        }
        cache.ready = SUCCEEDED(hr);
        if (!cache.ready) for (auto& path : cache.path) SafeRelease(path);
        SafeRelease(factory); SafeRelease(face); SafeRelease(font);
        SafeRelease(family); SafeRelease(fonts);
    }

    // Digit outlines are font curves, rasterized with coverage antialiasing.
    // Stroke first, then fill: only half the stroke remains outside the glyph.
    // Eleven geometries per used style are built once, never on every tick.
    void Text(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* b,
              IDWriteTextFormat* format, const WCHAR* text,
              const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) const
    {
        const Glyphs& cache = m_glyphs[o_style];
        int digit = -1;
        if (format == m_time[o_style] && text[0] && !text[1]) {
            if (text[0] >= L'0' && text[0] <= L'9') digit = text[0] - L'0';
            else if (text[0] == LocTimeSep()[0] && !LocTimeSep()[1]) digit = 10;
        }
        if (cache.ready && digit >= 0) {
            D2D1_MATRIX_3X2_F base;
            rt->GetTransform(&base);
            float x = rect.left + (rect.right - rect.left - cache.advance[digit]) / 2;
            rt->SetTransform(D2D1::Matrix3x2F::Translation(x, cache.baseline) * base);
            rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            b->SetColor(D2D1::ColorF(0.04f, 0.05f, 0.06f, 1.0f));
            rt->DrawGeometry(cache.path[digit], b, 1.25f);
            b->SetColor(color);
            rt->FillGeometry(cache.path[digit], b);
            rt->SetTransform(base);
            return;
        }
        b->SetColor(color);
        DrawTextIn(rt, b, format, text, rect);
    }

public:
    const WCHAR* GetName() const override { return L"Digital"; }
    UINT GetLabel() const override { return IDS_FACE_DIGITAL; }

    bool DrawDial(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* b) const override
    {
        if (o_panel) {
            b->SetColor(D2D1::ColorF(0.05f, 0.07f, 0.12f, 0.35f));
            rt->FillRoundedRectangle(D2D1::RoundedRect(DigitalBounds(), 8, 8), b);
        }
        return true;
    }

    int GetOptions(const FaceOpt** out) const override
    {
        *out = s_opts;
        return _countof(s_opts);
    }

    void DrawHands(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* b,
                   const SYSTEMTIME& st, bool seconds,
                   const D2D1_MATRIX_3X2_F& base) const override
    {
        IDWriteFactory* dw = SharedDWrite();
        if (!dw) return;
        CacheGlyphs(rt);
        const Typography& style = STYLES[o_style];
        if (!m_time[o_style])
            m_time[o_style] = MakeTextFormat(dw, style.family, style.size,
                style.weight, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (!m_day)
            m_day = MakeTextFormat(dw, L"Segoe UI", 22, DWRITE_FONT_WEIGHT_NORMAL,
                                  DWRITE_TEXT_ALIGNMENT_CENTER);
        if (!m_label)
            m_label = MakeTextFormat(dw, L"Segoe UI", 10, DWRITE_FONT_WEIGHT_NORMAL,
                                    DWRITE_TEXT_ALIGNMENT_CENTER);

        rt->SetTransform(base);
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        const Bar bar = LayOut();
        const D2D1_COLOR_F bright = FromRGB(o_text), muted = FromRGB(o_muted);
        WCHAR buf[16];
        if (o_date) {
            float x = bar.x0 + 6;
            wsprintfW(buf, L"%02d", st.wDay);
            Text(rt, b, m_day, buf, D2D1::RectF(x, BY0, x + 30, BY1), bright);
            Text(rt, b, m_label, MONTH_ABBR[(st.wMonth - 1) % 12],
                 D2D1::RectF(x + 30, BMID - 14, x + 62, BMID), muted);
            Text(rt, b, m_label, DAY_ABBR[st.wDayOfWeek % 7],
                 D2D1::RectF(x + 30, BMID, x + 62, BMID + 14), muted);
        }

        int h = o_hour24 ? st.wHour : (st.wHour % 12 ? st.wHour % 12 : 12);
        wsprintfW(buf, L"%02d%02d%02d", h, st.wMinute, st.wSecond);
        float x = bar.time;
        for (int i = 0; i < (seconds ? 6 : 4); i++) {
            if (i == 2 || i == 4) {
                Text(rt, b, m_time[o_style], LocTimeSep(),
                     D2D1::RectF(x, BY0, x + style.colon, BY1), bright);
                x += style.colon;
            }
            WCHAR digit[] = { buf[i], 0 };
            Text(rt, b, m_time[o_style], digit,
                 D2D1::RectF(x, BY0, x + style.cell, BY1),
                 bright);
            x += style.cell;
        }

        if (o_ampm)
            Text(rt, b, m_label, LocAmPm(st.wHour >= 12),
                 D2D1::RectF(bar.side, BY0, bar.side + SIDE_W, BY1), muted);
    }
};

static const DigitalFace s_digital;
extern const IClockFace* const g_faceDigital = &s_digital;
