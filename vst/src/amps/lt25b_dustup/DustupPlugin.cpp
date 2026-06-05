/*
 * Dustup CDN — Ashdown ABM EVO (Bass Magnifier) bass head, COMPONENT-LEVEL model.
 *
 * Built from the Ashdown ABM500 EVO preamp schematic (Bass Magnifier, APC010):
 *   • INPUT  — Passive/Active + Push Flat/Shape; input gain
 *   • VALVE  — ECC83 (12AX7) clean->grind blend (the Ashdown tube warmth/drive)
 *   • TONE   — Bass / Middle / Treble knobs + a 6-band graphic EQ
 *              (100/180/340/1.3k/3.6k/5k, EQ In/Out)
 *   • SUB    — sub-octave generator (analog-style zero-cross divider) for weight
 *   • COMP   — built-in compressor (on/off + amount)
 *   • OUTPUT — master into the SS power amp (clean)
 *
 * The ECC83 is the REAL nodal triode; the graphic EQ is the nodal multiple-
 * feedback band engine; the Sub is a half-rate zero-cross square tracked by an
 * envelope (a real analog-divider style sub-octave).
 */
#include "DistrhoPlugin.hpp"
#include "DustupParams.h"
#include <cmath>

START_NAMESPACE_DISTRHO

static inline float rbAmpLvl(float x){ const float t=0.90f,c=0.99f,a=(x<0.f?-x:x);
    if(a<=t) return x; return (x<0.f?-1.f:1.f)*(t+(c-t)*std::tanh((a-t)/(c-t))); }
static inline float softClip(float x) { return std::tanh(x); }

// ── RBJ biquad — tone stack + Shape ──────────────────────────────────────────
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
        if (fc > fs*0.49f) fc = fs*0.49f;
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

// ── Tiny fixed-size MNA solver (RT-safe) — 12AX7 + graphic-EQ bands ───────────
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
    inline void OpAmp(int np, int nnode, int no, int k) { int r = nn+k;
        if (no>0) A[(no-1)*sz+r] += 1; if (np>0) A[r*sz+(np-1)] += 1; if (nnode>0) A[r*sz+(nnode-1)] -= 1; }
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

// ── ECC83 (12AX7) triode (Valve Drive) — nodal Koren + Newton/sample ─────────
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

// ── Multiple-feedback band-pass — one graphic-EQ band (same as Silla/HB5000) ──
struct MFB {
    double R1=1, R2=1, R3=1, Cc=1e-8, c1v=0, c1i=0, c2v=0, c2i=0, T=1.0/48000.0;
    void setT(float fs) { T = 1.0/((fs>0.f)?fs:48000.0); }
    void set(double fc, double Q, double C) { Cc=C; const double w=6.2831853*fc;
        R1=Q/(w*C); R2=Q/((2*Q*Q-1.0)*w*C); R3=2*Q/(w*C); }
    void reset() { c1v=c1i=c2v=c2i=0; }
    inline double proc(double in) {
        const double G=2.0*Cc/T, I1=G*c1v+c1i, I2=G*c2v+c2i;
        Mna m; m.init(4,2);
        m.Vsrc(1,in,0); m.R(1,2,R1); m.R(2,0,R2); m.R(3,4,R3);
        m.stampG(2,4,G); m.Isrc(4,2,I1);
        m.stampG(2,3,G); m.Isrc(3,2,I2);
        m.OpAmp(0,4,3,1);
        if (!m.solve()) return 0.0;
        const double out=m.x[2];
        { const double v=m.x[1]-m.x[3], i=G*(v-c1v)-c1i; c1i=i; c1v=v; }
        { const double v=m.x[1]-m.x[2], i=G*(v-c2v)-c2i; c2i=i; c2v=v; }
        return out;
    }
};

// ── Sub-octave generator — analog-style half-rate zero-cross divider ─────────
struct SubOct {
    float lp=0, prev=0, flip=1, env=0, out=0, alp=0.02f, aout=0.02f;
    void setSR(float fs){ alp=1.f-std::exp(-6.2831853f*180.f/fs); aout=1.f-std::exp(-6.2831853f*110.f/fs); }
    void reset(){ lp=prev=env=out=0; flip=1; }
    inline float process(float x, float level){
        lp += alp * (x - lp);                       // low-pass tracker (~180 Hz)
        if (lp > 0.f && prev <= 0.f) flip = -flip;   // rising zero-cross -> toggle (octave down)
        prev = lp;
        const float e = std::fabs(lp); env += 0.01f * (e - env);
        out += aout * (flip * env - out);            // smoothed half-rate square
        return level * out;
    }
};

// ── gentle opto compressor (on/off + amount) ─────────────────────────────────
struct OptoComp {
    float env=0, atk=0, rel=0;
    void setSR(float fs){ atk=std::exp(-1.f/(0.005f*fs)); rel=std::exp(-1.f/(0.130f*fs)); }
    void reset(){ env=0; }
    inline float process(float x, float amount){
        const float a=std::fabs(x); const float c=(a>env)?atk:rel; env=c*env+(1.f-c)*a;
        const float thr=0.22f; float gr=1.f;
        if (env>thr){ const float over=env-thr; gr=1.f/(1.f+amount*5.0f*over); }
        return x*gr*(1.f+amount*0.30f);
    }
};

class DustupChannel {
    float fs = 48000.f;
    Triode v1;
    Biquad shapeMid, shapeLo, shapeHi;        // Push Flat/Shape contour
    Biquad bqBass, bqMid, bqTreble;           // Bass/Middle/Treble
    MFB eq[kNumEq]; float eqG[kNumEq];        // 6-band graphic EQ
    Biquad subLp;                             // sub low-pass shaping
    SubOct sub; OptoComp comp;
    float drive=1, output=1, subLvl=0, compAmt=0;
    bool shape=false, eqIn=true, subOn=false, compOn=false;
public:
    void setSampleRate(float s){ fs=(s>0.f)?s:48000.f; v1.setT(s); sub.setSR(fs); comp.setSR(fs);
        for (int i=0;i<kNumEq;++i){ eq[i].setT(s); eq[i].set(kEqFreqs[i], 1.4, 1e-8); eqG[i]=1.f; } }
    void reset(){ v1.reset(); shapeMid.reset(); shapeLo.reset(); shapeHi.reset();
        bqBass.reset(); bqMid.reset(); bqTreble.reset(); subLp.reset(); sub.reset(); comp.reset();
        for (int i=0;i<kNumEq;++i) eq[i].reset(); }

    void setParams(const float* p) {
        const float pad = (p[kActive] > 0.5f) ? 0.45f : 1.0f;
        // Input gain + Valve Drive both push the ECC83 (clean -> grind).
        drive = (0.4f + p[kInput] * 5.0f) * (0.5f + p[kValve] * 6.0f) * pad;

        shape = p[kShape] > 0.5f;                         // fixed mid-scoop "smile"
        shapeMid.setPeak(500.f, shape ? -9.0f : 0.f, 0.7f, fs);
        shapeLo.setLowShelf(80.f, shape ? 4.0f : 0.f, fs);
        shapeHi.setHighShelf(3500.f, shape ? 4.0f : 0.f, fs);

        bqBass.setLowShelf(60.f, (p[kBass]-0.5f)*30.f, fs);
        bqMid.setPeak(660.f, (p[kMiddle]-0.5f)*24.f, 0.7f, fs);
        bqTreble.setHighShelf(5000.f, (p[kTreble]-0.5f)*30.f, fs);

        eqIn = p[kEqIn] > 0.5f;
        for (int i=0;i<kNumEq;++i)
            eqG[i] = eqIn ? std::pow(10.f, (p[kFirstEq+i]-0.5f)*24.f/20.f) : 1.f;   // +/-12 dB

        subOn = p[kSubOn] > 0.5f; subLvl = p[kSub] * 1.4f;
        subLp.setLowpassQ(120.f, 0.7f, fs);
        compOn = p[kCompOn] > 0.5f; compAmt = p[kComp];

        output = p[kOutput] / 0.7f;
    }

    inline float process(float x) {
        float s = (float)v1.process((double)(drive * x));        // Input + Valve Drive (ECC83)
        s = shapeMid.process(s); s = shapeLo.process(s); s = shapeHi.process(s);  // Shape
        s = bqBass.process(s); s = bqMid.process(s); s = bqTreble.process(s);     // tone
        if (eqIn) { const double dry=s; double sum=dry;
            for (int i=0;i<kNumEq;++i) sum -= (eqG[i]-1.0) * eq[i].proc(dry); s = (float)sum; }  // graphic EQ
        if (subOn) s += subLp.process(sub.process(s, subLvl));   // Sub Harmonics
        if (compOn) s = comp.process(s, compAmt);                // Comp
        return s * output;                                       // Output (SS power, clean)
    }
};

static constexpr float kDustupMakeup = 4.45f;   // tuned offline (~-14 dBFS @ noon)
static constexpr float kDustupLvl    = 0.2563f;

class DustupPlugin : public Plugin {
    DustupChannel L, R;
    float fParams[kParamCount];
    void recalc(){ L.setParams(fParams); R.setParams(fParams); }
public:
    DustupPlugin() : Plugin(kParamCount, 0, 0) {
        for (int i=0;i<kParamCount;++i) fParams[i]=kDustupDef[i];
        const float sr=(float)getSampleRate();
        L.setSampleRate(sr); R.setSampleRate(sr); L.reset(); R.reset(); recalc();
    }
protected:
    const char* getLabel()       const override { return "DustupCDN"; }
    const char* getDescription() const override { return "Ashdown ABM EVO Bass Magnifier head — component-level model"; }
    const char* getMaker()       const override { return "RigBuilder"; }
    const char* getLicense()     const override { return "ISC"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 0); }
    int64_t     getUniqueId()    const override { return d_cconst('R', 'B', 'D', 'U'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if (i >= (uint32_t)kParamCount) return;
        p.hints = kParameterIsAutomatable;
        if (i >= (uint32_t)kActive) p.hints |= kParameterIsBoolean;
        p.name = kDustupNames[i]; p.symbol = kDustupSymbols[i];
        p.ranges.min = kDustupMin[i]; p.ranges.max = kDustupMax[i]; p.ranges.def = kDustupDef[i];
    }
    float getParameterValue(uint32_t i) const override { return (i < (uint32_t)kParamCount) ? fParams[i] : 0.f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < (uint32_t)kParamCount) { fParams[i]=v; recalc(); } }
    void  sampleRateChanged(double r) override { L.setSampleRate((float)r); R.setSampleRate((float)r); L.reset(); R.reset(); recalc(); }

    void run(const float** in, float** out, uint32_t frames) override {
        const float* iL=in[0]; const float* iR=in[1]; float* oL=out[0]; float* oR=out[1];
        for (uint32_t i=0;i<frames;++i){ oL[i]=rbAmpLvl(kDustupLvl*softClip(kDustupMakeup*L.process(iL[i]))*0.98f); oR[i]=rbAmpLvl(kDustupLvl*softClip(kDustupMakeup*R.process(iR[i]))*0.98f); }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DustupPlugin)
};

Plugin* createPlugin() { return new DustupPlugin(); }

END_NAMESPACE_DISTRHO
