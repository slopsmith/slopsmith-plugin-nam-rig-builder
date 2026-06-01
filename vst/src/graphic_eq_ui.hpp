/*
 * Shared graphic-EQ UI (DPF NanoVG) — styled like a Boss GE-7 / GE-7B compact:
 * coloured body, a recessed dark panel of vertical faders (one per band), and a
 * big treadle footswitch at the bottom carrying the engraved name. Drag a fader
 * up/down to cut/boost. Copyright-free (no brand/model name on the pedal).
 *
 * The per-pedal Bands.h defines kEqBands/kEqFreqs/kEqNames/EQ_PLUGIN_LABEL/EQ_DB
 * and the body colour EQ_ACR/EQ_ACG/EQ_ACB (the real pedal's colour).
 */
#include "DistrhoUI.hpp"
#include "_shared/fonts_data.hpp"
#include <cmath>
#include <cstdio>

#ifndef EQ_ACR
#define EQ_ACR 198
#define EQ_ACG 200
#define EQ_ACB 206
#endif

START_NAMESPACE_DISTRHO

class GraphicEqUI : public UI
{
    float fValues[kEqBands];
    int   fDrag;
    bool  fEditing = false;
    int   fName = -1, fLbl = -1;

    float scale()  const { return getWidth() / 460.0f; }
    float panelX() const { return getWidth()  * 0.07f; }
    float panelW() const { return getWidth()  * 0.86f; }
    float panelY() const { return getHeight() * 0.135f; }
    float panelH() const { return getHeight() * 0.40f; }
    float colW()   const { return panelW() / (float)kEqBands; }
    float colX(int i) const { return panelX() + (i + 0.5f) * colW(); }
    float trackTop()    const { return panelY() + getHeight() * 0.055f; }
    float trackBottom() const { return panelY() + panelH() - getHeight() * 0.055f; }

    float valToY(float v) const { return trackTop() + (1.0f - v) * (trackBottom() - trackTop()); }
    float yToVal(double y) const {
        float v = 1.0f - (float)((y - trackTop()) / (trackBottom() - trackTop()));
        return v < 0.f ? 0.f : v > 1.f ? 1.f : v;
    }
    int colAt(double px, double py) const {
        if (py < panelY() || py > panelY() + panelH()) return -1;
        int i = (int)((px - panelX()) / colW());
        return (i >= 0 && i < kEqBands) ? i : -1;
    }
    Color bodyText() const {
        const float lum = 0.299f*EQ_ACR + 0.587f*EQ_ACG + 0.114f*EQ_ACB;
        return lum > 140.f ? Color(34, 34, 38) : Color(238, 240, 246);
    }
    static int cl(int v){ return v<0?0:(v>255?255:v); }
public:
    GraphicEqUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT), fDrag(-1) {
        loadSharedResources();
        fName = createFontFromMemory("pk_serif",  pk_serif_ttf,  pk_serif_ttf_len,  false); // Crete Round
        fLbl  = createFontFromMemory("pk_barlow", pk_barlow_ttf, pk_barlow_ttf_len, false);
        for (int i = 0; i < kEqBands; ++i) fValues[i] = 0.5f;
        setGeometryConstraints(360, 280, true, false);
    }
protected:
    void parameterChanged(uint32_t index, float value) override {
        if (index < (uint32_t)kEqBands) { fValues[index] = value; repaint(); }
    }
    void onNanoDisplay() override {
        const float W = getWidth(), H = getHeight(), f = scale(), m = 8*f;
        const int R = EQ_ACR, G = EQ_ACG, B = EQ_ACB;
        // backdrop + body
        beginPath(); rect(0, 0, W, H); fillColor(Color(11, 11, 13)); fill();
        Paint body = linearGradient(0, m, 0, H-m, Color(cl(R+16),cl(G+16),cl(B+16)), Color(cl(R-14),cl(G-14),cl(B-14)));
        beginPath(); roundedRect(m, m, W-2*m, H-2*m, 14*f); fillPaint(body); fill();
        beginPath(); roundedRect(m, m, W-2*m, H-2*m, 14*f); strokeColor(Color(0,0,0,110)); strokeWidth(2*f); stroke();

        const Color tc = bodyText();
        // small top label + LED
        fontFaceId(fLbl); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fontSize(11*f); fillColor(tc); text(W*0.08f, H*0.075f, "GRAPHIC  EQUALIZER", NULL);
        beginPath(); circle(W*0.90f, H*0.075f, 4.5f*f); fillColor(Color(224,70,58)); fill();

        // recessed fader panel
        beginPath(); roundedRect(panelX(), panelY(), panelW(), panelH(), 8*f); fillColor(Color(22, 23, 27)); fill();
        beginPath(); roundedRect(panelX(), panelY(), panelW(), panelH(), 8*f); strokeColor(Color(0,0,0,120)); strokeWidth(1.5f*f); stroke();

        const float tT = trackTop(), tB = trackBottom(), midY = (tT + tB) * 0.5f;
        for (int i = 0; i < kEqBands; ++i) {
            const float cx = colX(i);
            beginPath(); roundedRect(cx - 2.5f*f, tT, 5*f, tB - tT, 2.5f*f); fillColor(Color(50, 52, 60)); fill();
            beginPath(); rect(cx - 6*f, midY, 12*f, 1.0f*f); fillColor(Color(86, 90, 104)); fill();
            const float hy = valToY(fValues[i]);
            const float y0 = std::fmin(hy, midY), y1 = std::fmax(hy, midY);
            beginPath(); roundedRect(cx - 2.5f*f, y0, 5*f, y1 - y0, 2.5f*f); fillColor(Color(cl(R-20),cl(G-20),cl(B-20))); fill();
            // fader cap (metal)
            Paint capp = linearGradient(0, hy-5*f, 0, hy+5*f, Color(232,234,238), Color(150,153,160));
            beginPath(); roundedRect(cx - 11*f, hy - 5*f, 22*f, 10*f, 2.5f*f); fillPaint(capp); fill();
            beginPath(); rect(cx - 11*f, hy-0.5f*f, 22*f, 1.0f*f); fillColor(Color(60,62,68)); fill();
            // freq label below the panel
            fontFaceId(fLbl); textAlign(ALIGN_CENTER | ALIGN_TOP);
            fontSize(8.5f*f); fillColor(tc); text(cx, panelY() + panelH() + 3*f, kEqNames[i], NULL);
        }

        // ── treadle footswitch (the raised pad) with the engraved name ──
        const float tx = m+4*f, tw = W-2*m-8*f, tyTop = H*0.62f, tBot = H - m - 6*f;
        Paint tre = linearGradient(0, tyTop, 0, tBot, Color(cl(R-2),cl(G-2),cl(B-2)), Color(cl(R-16),cl(G-16),cl(B-16)));
        beginPath(); roundedRect(tx, tyTop, tw, tBot - tyTop, 12*f); fillPaint(tre); fill();
        beginPath(); roundedRect(tx, tyTop, tw, 10*f, 12*f); fillColor(Color(255,255,255,20)); fill();
        beginPath(); roundedRect(tx, tyTop, tw, tBot - tyTop, 12*f); strokeColor(Color(0,0,0,120)); strokeWidth(1.6f*f); stroke();
        // black step pad (lower part of the treadle)
        beginPath(); roundedRect(tx+12*f, H*0.82f, tw-24*f, tBot-9*f-H*0.82f, 9*f); fillColor(Color(20,20,22)); fill();
        // engraved name (Crete Round) on the upper treadle
        fontFaceId(fName >= 0 ? fName : fLbl); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fontSize(34*f); fillColor(Color(16,16,20)); text(W*0.5f, H*0.71f, EQ_PLUGIN_LABEL, NULL);
    }
    bool onMouse(const MouseEvent& ev) override {
        if (ev.button != 1) return false;
        if (ev.press) {
            const int i = colAt(ev.pos.getX(), ev.pos.getY());
            if (i >= 0) { fDrag = i; setFromY(i, ev.pos.getY()); return true; }
        } else if (fDrag >= 0) { editParameter(fDrag, false); fEditing = false; fDrag = -1; return true; }
        return false;
    }
    bool onMotion(const MotionEvent& ev) override {
        if (fDrag >= 0) { setFromY(fDrag, ev.pos.getY()); return true; }
        return false;
    }
private:
    void setFromY(int i, double y) {
        if (!fEditing) { editParameter(i, true); fEditing = true; }
        const float v = yToVal(y);
        fValues[i] = v; setParameterValue(i, v); repaint();
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicEqUI)
};

UI* createUI() { return new GraphicEqUI(); }

END_NAMESPACE_DISTRHO
