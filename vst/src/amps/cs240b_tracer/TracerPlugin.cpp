/*
 * Tracer V8 — Trace Elliot V-Type V8 (400 W all-valve bass head), COMPONENT-LEVEL.
 *
 * Built from the Trace Elliot V-Type V8 schematic (cd0119/cd0120, 2000):
 *   • INPUT  — Passive / Active (Active pads hot active basses)
 *   • PREAMP — all-ECC83 (12AX7): Gain I (V1, + Bright cap) → Gain II (V2, a Pull
 *              switch adds a hotter stage) → Level
 *   • TONE   — passive Bass / Middle / Treble; Bass has a Deep pull (deeper low
 *              shelf), Middle has a Shift pull (mid centre moves up)
 *   • COMP   — the built-in opto compressor (On/Off + Level)
 *   • POWER  — 8x KT88 push-pull (~400 W): enormous clean headroom, late knee
 *
 * Two REAL nodal 12AX7 stages (Koren + Newton/sample) feed a white-boxed Trace
 * tone stack and a very-late-knee KT88 power stage.
 */
#include "DistrhoPlugin.hpp"
#include "TracerParams.h"
#include <cmath>

START_NAMESPACE_DISTRHO

static inline float rbAmpLvl(float x){ const float t=0.90f,c=0.99f,a=(x<0.f?-x:x);
    if(a<=t) return x; return (x<0.f?-1.f:1.f)*(t+(c-t)*std::tanh((a-t)/(c-t))); }
static inline float softClip(float x) { return std::tanh(x); }

// ── RBJ biquad — tone stack + bright ─────────────────────────────────────────
class Biquad {
    float b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
public:
    void reset() { z1 = z2 = 0.f; }
    inline float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setLowShelf(float fc, float dB, float fs) {
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw * 0.5f * 1.4142135f;
        const float sA = std::sqrt(A), tsAa = 2.f * sA * alpha;
        const float a0 =       (A + 1) + (A - 1) * cw + tsAa;
        b0 =  A * ((A + 1) - (A - 1) * cw + tsAa) / a0;
        b1 = 2*A * ((A - 1) - (A + 1) * cw)        / a0;
        b2 =  A * ((A + 1) - (A - 1) * cw - tsAa)  / a0;
        a1 = -2 * ((A - 1) + (A + 1) * cw)         / a0;
        a2 =      ((A + 1) + (A - 1) * cw - tsAa)  / a0;
    }
    void setHighShelf(float fc, float dB, float fs) {
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw * 0.5f * 1.4142135f;
        const float sA = std::sqrt(A), tsAa = 2.f * sA * alpha;
        const float a0 =       (A + 1) - (A - 1) * cw + tsAa;
        b0 =  A * ((A + 1) + (A - 1) * cw + tsAa) / a0;
        b1 = -2*A * ((A - 1) + (A + 1) * cw)      / a0;
        b2 =  A * ((A + 1) + (A - 1) * cw - tsAa) / a0;
        a1 =  2 * ((A - 1) - (A + 1) * cw)        / a0;
        a2 =      ((A + 1) - (A - 1) * cw - tsAa) / a0;
    }
    void setPeak(float fc, float dB, float Q, float fs) {
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0; b1 = (-2 * cw) / a0; b2 = (1 - alpha * A) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha / A) / a0;
    }
    void setLowpassQ(float fc, float Q, float fs) {
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha;
        b0 =  (1 - cw) * 0.5f / a0; b1 = (1 - cw) / a0; b2 = (1 - cw) * 0.5f / a0;
        a1 =  -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
    void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
};

// ── Tiny fixed-size MNA solver (RT-safe) — for the 12AX7 stages ───────────────
struct Mna {
    static const int MAXN = 8;
    int sz, nn;
    double A[MAXN*MAXN], b[MAXN], x[MAXN];
    void init(int nN, int nX) { nn = nN; sz = nN + nX;
        for (int i = 0; i < sz*sz; ++i) A[i] = 0.0; for (int i = 0; i < sz; ++i) b[i] = 0.0; }
    inline void stampG(int a, int bb, double g) {
        if (a>0)  { A[(a-1)*sz+(a-1)]  += g; if (bb>0) A[(a-1)*sz+(bb-1)] -= g; }
        if (bb>0) { A[(bb-1)*sz+(bb-1)]+= g; if (a>0)  A[(bb-1)*sz+(a-1)] -= g; } }
    inline void R(int a, int bb, double r) { if (r < 1e-9) r = 1e-9; stampG(a, bb, 1.0/r); }
    inline void Isrc(int a, int bb, double I) { if (a>0) b[a-1] -= I; if (bb>0) b[bb-1] += I; }
    inline void Vsrc(int a, double V, int k) { int r = nn+k;
        if (a>0) { A[(a-1)*sz+r] += 1; A[r*sz+(a-1)] += 1; } b[r] = V; }
    inline void gm(int oa, int ob, int ca, int cb, double g) {
        if (oa>0) { if (ca>0) A[(oa-1)*sz+(ca-1)] += g; if (cb>0) A[(oa-1)*sz+(cb-1)] -= g; }
        if (ob>0) { if (ca>0) A[(ob-1)*sz+(ca-1)] -= g; if (cb>0) A[(ob-1)*sz+(cb-1)] += g; } }
    bool solve() { const int n = sz;
        for (int col = 0; col < n; ++col) {
            int piv = col; double mx = std::fabs(A[col*n+col]);
            for (int r = col+1; r < n; ++r) { double v = std::fabs(A[r*n+col]); if (v > mx) { mx = v; piv = r; } }
            if (mx < 1e-18) return false;
            if (piv != col) { for (int c = 0; c < n; ++c) { double t = A[col*n+c]; A[col*n+c] = A[piv*n+c]; A[piv*n+c] = t; }
                double t = b[col]; b[col] = b[piv]; b[piv] = t; }
            const double d = A[col*n+col];
            for (int r = 0; r < n; ++r) { if (r == col) continue; const double f = A[r*n+col]/d; if (f == 0) continue;
                for (int c = col; c < n; ++c) A[r*n+c] -= f*A[col*n+c]; b[r] -= f*b[col]; } }
        for (int i = 0; i < n; ++i) x[i] = b[i] / A[i*n+i];
        return true; } };

// ── ECC83 (12AX7) triode — nodal Koren + Newton/sample ───────────────────────
struct Triode {
    double vG=0, vP=300, vK=1.5, dcAvg=300.0, T=1.0/48000.0;
    void setT(float fs) { T = 1.0 / ((fs>0.f)?fs:48000.0); }
    void reset() { vG=0; vP=300; vK=1.5; dcAvg=300.0; }
    static inline double Ip(double vgk, double vpk) {
        const double MU=100, EX=1.4, KG1=1060, KP=600, KVB=300;
        if (vpk < 0) vpk = 0;
        double e1 = (vpk/KP)*std::log(1.0 + std::exp(KP*(1.0/MU + vgk/std::sqrt(KVB + vpk*vpk))));
        if (e1 < 0) e1 = 0; return std::pow(e1, EX)/KG1*2.0; }
    inline double process(double vin) {
        const double Bp=300, Rp=100000, Rk=1500, h=1e-4;
        double G=vG, P=vP, K=vK;
        for (int it=0; it<12; ++it) {
            Mna m; m.init(4, 2);
            m.Vsrc(1, Bp, 0); m.Vsrc(2, vin, 1);
            m.R(3, 1, Rp); m.R(4, 0, Rk);
            const double vgk=G-K, vpk=P-K, ip=Ip(vgk,vpk);
            const double gmv=(Ip(vgk+h,vpk)-ip)/h, gp=(Ip(vgk,vpk+h)-ip)/h;
            m.gm(3,0,2,0,gmv); m.gm(3,0,3,0,gp); m.gm(3,0,4,0,-(gmv+gp));
            m.gm(4,0,2,0,-gmv); m.gm(4,0,3,0,-gp); m.gm(4,0,4,0,(gmv+gp));
            m.Isrc(3,0, ip-(gmv*G+gp*P-(gmv+gp)*K));
            m.Isrc(4,0, -ip-(-gmv*G-gp*P+(gmv+gp)*K));
            if (!m.solve()) break;
            const double nP=m.x[2], nK=m.x[3]; const double err=std::fabs(nP-P)+std::fabs(nK-K);
            G=m.x[1]; P=P+0.7*(nP-P); K=K+0.7*(nK-K);
            if (err<1e-6) break;
        }
        if (!std::isfinite(P)) { reset(); return 0.0; }
        vG=G; vP=P; vK=K;
        dcAvg += 0.0008*(P-dcAvg);
        return -(P - dcAvg) * (1.0/40.0);
    }
};

// ── built-in opto compressor (On/Off + Level) ────────────────────────────────
struct OptoComp {
    float env=0, atk=0, rel=0;
    void setSR(float fs){ atk=std::exp(-1.f/(0.005f*fs)); rel=std::exp(-1.f/(0.140f*fs)); }
    void reset(){ env=0; }
    inline float process(float x, float amount){
        const float a=std::fabs(x); const float c=(a>env)?atk:rel; env=c*env+(1.f-c)*a;
        const float thr=0.22f; float gr=1.f;
        if (env>thr){ const float over=env-thr; gr=1.f/(1.f+amount*5.0f*over); }
        return x*gr*(1.f+amount*0.30f);
    }
};

class TracerChannel {
    float fs = 48000.f;
    Triode v1, v2;                            // Gain I + Gain II (ECC83)
    Biquad brightHS;                          // Gain I bright cap
    Biquad bqBass, bqMid, bqTreble, pwrLP;    // tone stack + OT band-limit
    OptoComp comp;
    float d1=1, d2=1, level=1, master=1, pwrDrive=1, compAmt=0;
    bool bright=false, compOn=false;

    // 8x KT88 push-pull (~400 W): symmetric soft clip with a VERY late knee —
    // enormous clean headroom (the V8 stays clean until truly cranked).
    static inline float pushPull(float x) {
        return std::tanh(x * 0.55f) * 1.8182f;   // 1/0.55 make-up so small x ≈ unity
    }
public:
    void setSampleRate(float s){ fs=(s>0.f)?s:48000.f; v1.setT(s); v2.setT(s); comp.setSR(fs); }
    void reset(){ v1.reset(); v2.reset(); brightHS.reset(); bqBass.reset(); bqMid.reset(); bqTreble.reset(); pwrLP.reset(); comp.reset(); }

    void setParams(const float* p) {
        const float pad = (p[kActive] > 0.5f) ? 0.45f : 1.0f;
        d1 = (0.4f + p[kGain1] * 8.0f) * pad;                          // Gain I
        bright = p[kBright] > 0.5f;
        if (bright) brightHS.setHighShelf(2500.f, 6.0f, fs); else brightHS.setBypass();
        const bool g2pull = p[kGain2Pull] > 0.5f;
        d2 = 0.5f + p[kGain2] * (g2pull ? 12.0f : 7.0f);              // Gain II (Pull = hotter)
        level = p[kLevel] * 1.4f;

        // Trace tone stack (+/-15 dB). Deep pull -> deeper low shelf; Mid Shift
        // pull -> mid centre moves up (500 -> 1.2 kHz).
        const bool deep = p[kDeep] > 0.5f, shift = p[kMidShift] > 0.5f;
        bqBass.setLowShelf(deep ? 45.f : 80.f, (p[kBass]-0.5f)*30.f + (deep?4.f:0.f), fs);
        bqMid.setPeak(shift ? 1200.f : 500.f, (p[kMiddle]-0.5f)*24.f, 0.8f, fs);
        bqTreble.setHighShelf(4000.f, (p[kTreble]-0.5f)*30.f, fs);

        compOn = p[kCompOn] > 0.5f; compAmt = p[kComp];

        master = p[kMaster] / 0.7f;
        pwrDrive = 0.5f + master * 0.85f;
        pwrLP.setLowpassQ(9000.f, 0.7f, fs);
    }

    inline float process(float x) {
        float s = (float)v1.process((double)(d1 * x));      // Gain I (ECC83)
        s = brightHS.process(s);                            // Bright cap
        s = (float)v2.process((double)(d2 * s)) * level;    // Gain II (ECC83) + Level
        s = bqBass.process(s); s = bqMid.process(s); s = bqTreble.process(s);  // tone
        if (compOn) s = comp.process(s, compAmt);           // compressor
        s = pushPull(s * pwrDrive) * master;                // 8x KT88 power
        s = pwrLP.process(s);
        return s;
    }
};

static constexpr float kTracerMakeup = 1.45f;   // tuned offline (~-15 dBFS @ noon)
static constexpr float kTracerLvl    = 0.2775f;

class TracerPlugin : public Plugin {
    TracerChannel L, R;
    float fParams[kParamCount];
    void recalc(){ L.setParams(fParams); R.setParams(fParams); }
public:
    TracerPlugin() : Plugin(kParamCount, 0, 0) {
        for (int i=0;i<kParamCount;++i) fParams[i]=kTracerDef[i];
        const float sr=(float)getSampleRate();
        L.setSampleRate(sr); R.setSampleRate(sr); L.reset(); R.reset(); recalc();
    }
protected:
    const char* getLabel()       const override { return "TracerV8"; }
    const char* getDescription() const override { return "Trace Elliot V-Type V8 all-valve bass head — component-level model"; }
    const char* getMaker()       const override { return "RigBuilder"; }
    const char* getLicense()     const override { return "ISC"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 0); }
    int64_t     getUniqueId()    const override { return d_cconst('R', 'B', 'V', '8'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if (i >= (uint32_t)kParamCount) return;
        p.hints = kParameterIsAutomatable;
        if (i >= (uint32_t)kActive) p.hints |= kParameterIsBoolean;
        p.name = kTracerNames[i]; p.symbol = kTracerSymbols[i];
        p.ranges.min = kTracerMin[i]; p.ranges.max = kTracerMax[i]; p.ranges.def = kTracerDef[i];
    }
    float getParameterValue(uint32_t i) const override { return (i < (uint32_t)kParamCount) ? fParams[i] : 0.f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < (uint32_t)kParamCount) { fParams[i]=v; recalc(); } }
    void  sampleRateChanged(double r) override { L.setSampleRate((float)r); R.setSampleRate((float)r); L.reset(); R.reset(); recalc(); }

    void run(const float** in, float** out, uint32_t frames) override {
        const float* iL=in[0]; const float* iR=in[1]; float* oL=out[0]; float* oR=out[1];
        for (uint32_t i=0;i<frames;++i){ oL[i]=rbAmpLvl(kTracerLvl*softClip(kTracerMakeup*L.process(iL[i]))*0.98f); oR[i]=rbAmpLvl(kTracerLvl*softClip(kTracerMakeup*R.process(iR[i]))*0.98f); }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TracerPlugin)
};

Plugin* createPlugin() { return new TracerPlugin(); }

END_NAMESPACE_DISTRHO
