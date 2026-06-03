/*
 * Sharke HB3500 — Hartke HA3500 bass head model.
 *
 * From the HA3500 circuit diagram (Samson/Hartke, main board 400518280):
 *   1. Input  : Passive (-20 dB) or Active (-34 dB) jack — Active pads hot basses.
 *   2. Tube   : a 12AX7 preamp path (warm, soft-clipping) at the Tube level.
 *   3. Solid State : a clean/punchy op-amp preamp path at its level. The two
 *               paths SUM — the HA3500's signature blendable dual preamp.
 *   4. Compression : the built-in compressor, amount = Compression.
 *   5. 10-band graphic EQ : 30/64/125/250/500/1k/2k/4k/8k/16k Hz, +/-12 dB,
 *               engaged by EQ In.
 *   6. High Pass / Low Pass : variable HPF (20..200 Hz) and LPF (2k..20 kHz).
 *   7. Volume : master output.
 */
#include "DistrhoPlugin.hpp"
#include "HartkeParams.h"
#include <cmath>

START_NAMESPACE_DISTRHO

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
    void setPeak(float fc, float dB, float Q, float fs) {
        if (fc > fs * 0.49f) fc = fs * 0.49f;
        const float A = std::pow(10.f, dB / 40.f);
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0; b1 = (-2 * cw) / a0; b2 = (1 - alpha * A) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha / A) / a0;
    }
    void setLowPass(float fc, float Q, float fs) {
        if (fc > fs * 0.49f) fc = fs * 0.49f;
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha;
        b0 = ((1 - cw) * 0.5f) / a0; b1 = (1 - cw) / a0; b2 = ((1 - cw) * 0.5f) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha) / a0;
    }
    void setHighPass(float fc, float Q, float fs) {
        const float w0 = 6.2831853f * fc / fs, cw = std::cos(w0), sw = std::sin(w0);
        const float alpha = sw / (2.f * Q);
        const float a0 = 1 + alpha;
        b0 = ((1 + cw) * 0.5f) / a0; b1 = -(1 + cw) / a0; b2 = ((1 + cw) * 0.5f) / a0;
        a1 = (-2 * cw) / a0; a2 = (1 - alpha) / a0;
    }
    void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; z1 = z2 = 0; }
};

// ── Tiny fixed-size Modified Nodal Analysis solver (RT-safe, no heap) ─────────
// (Same engine used by the FK 800BR.) Node 0 = gnd; nodes 1..nN unknown
// voltages; nX aux currents. Resistors, capacitor companions, ideal op-amps and
// the transconductance stamp (gm) for nonlinear elements; solved by Gaussian
// elimination. Validated on RC low-pass, op-amp gain, and the 12AX7 triode.
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

// ── 12AX7 TUBE PREAMP — true nodal triode (Koren model + Newton/sample) ───────
// Common-cathode stage (B+ 300V, Rp 100k, Rk 1.5k self-bias, Ck cathode bypass).
// The Koren plate-current law Ip(Vgk,Vpk) is solved by Newton-Raphson each
// sample (numerical Jacobian, damped, warm-started); the plate swing clips
// ASYMMETRICALLY toward B+/cutoff = the warm tube character. Validated:
// bias ~0.99mA, gain ~x100, asymmetric overdrive.
struct Tube {
    double vG=0, vP=200, vK=1.4, ckv=0, cki=0, dcAvg=200.0, T=1.0/48000.0;
    void setT(float fs) { T = 1.0 / ((fs>0.f)?fs:48000.0); }
    void reset() { vG=0; vP=200; vK=1.4; ckv=0; cki=0; dcAvg=200.0; }
    static inline double Ip(double vgk, double vpk) {
        const double MU=100, EX=1.4, KG1=1060, KP=600, KVB=300;
        if (vpk < 0) vpk = 0;
        double e1 = (vpk/KP)*std::log(1.0 + std::exp(KP*(1.0/MU + vgk/std::sqrt(KVB + vpk*vpk))));
        if (e1 < 0) e1 = 0; return std::pow(e1, EX)/KG1*2.0; }
    inline double process(double vin) {        // vin = grid drive; returns AC plate swing
        const double Bp=300, Rp=100000, Rk=1500, Ck=1e-6, h=1e-4;
        const double Geq=2*Ck/T, Ieq=Geq*ckv+cki;
        double G=vG, P=vP, K=vK;
        for (int it=0; it<12; ++it) {
            Mna m; m.init(4, 2);                // 1 B+, 2 grid, 3 plate, 4 cathode
            m.Vsrc(1, Bp, 0); m.Vsrc(2, vin, 1);
            m.R(3, 1, Rp); m.R(4, 0, Rk);
            m.stampG(4, 0, Geq); m.Isrc(0, 4, Ieq);     // cathode bypass cap
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
        const double ci=Geq*(K-ckv)-cki; cki=ci; ckv=K;     // advance cathode cap
        vG=G; vP=P; vK=K;
        dcAvg += 0.0008*(P-dcAvg);
        return (P - dcAvg) * (1.0/100.0);                   // DC-blocked, ~unity small-signal
    }
};

// ── SOLID-STATE PREAMP — nodal non-inverting op-amp (gain ~11), rail clip ─────
struct SsPre {
    inline double process(double vin) {
        const double R2=4700, R3=47000, Vrail=13.5;
        Mna m; m.init(3, 2); m.Vsrc(1,vin,0); m.R(2,0,R2); m.R(3,2,R3); m.OpAmp(1,2,3,1);
        double vo=0; if (m.solve()) vo=m.x[2];
        if (std::fabs(vo)>Vrail) { Mna m2; m2.init(3,2); m2.Vsrc(1,vin,0); m2.R(2,0,R2); m2.R(3,2,R3);
            m2.Vsrc(3, (vo>0?Vrail:-Vrail), 1); if (m2.solve()) vo=m2.x[2]; }
        return vo * (1.0/11.0);                             // ~unity small-signal
    }
};

class HartkeChannel {
    float fs = 48000.f;
    Biquad eq[kNumEq];
    Biquad lpf, hpf;
    Tube tube; SsPre ss;                 // nodal dual preamp: real 12AX7 + op-amp SS
    float tubeDrive=1, ssDrive=1, master=1;
    bool  eqIn=true, lpfOn=false, hpfOn=false;
    // compressor (behavioral envelope follower — the gain-cell is not yet nodal)
    bool compOn=false; float compThr=1, compRatio=1, compMk=1, env=0, atk=0, rel=0;
    static inline float msC(float ms, float fs){ return std::exp(-1.f/(0.001f*ms*fs)); }
public:
    void setSampleRate(float s){ fs=(s>0)?s:48000.f; atk=msC(6.f,fs); rel=msC(120.f,fs); tube.setT(s); }
    void reset(){ tube.reset(); for(int i=0;i<kNumEq;++i) eq[i].reset(); lpf.reset(); hpf.reset(); env=0; }

    void setParams(const float* p) {
        const float padActive = (p[kActive] > 0.5f) ? 0.20f : 1.0f;   // Active jack ~ -14 dB
        tubeDrive = p[kTube]  * 2.0f * padActive;   // grid drive into the 12AX7
        ssDrive   = p[kSolid] * 2.0f * padActive;   // drive into the SS op-amp

        compOn = p[kComp] > 0.001f;
        compThr = 1.0f - p[kComp]*0.6f; compRatio = 1.0f + p[kComp]*5.0f; compMk = 1.0f + p[kComp]*0.7f;

        eqIn = p[kEqIn] > 0.5f;
        for (int i=0;i<kNumEq;++i) {
            if (eqIn) eq[i].setPeak(kEqFreqs[i], (p[kFirstEq+i]-0.5f)*24.f, 1.4f, fs);
            else      eq[i].setBypass();
        }
        // High Pass: 20 .. 200 Hz (0 = open/off). Low Pass: 2k .. 20k (1 = open/off).
        hpfOn = p[kHighPass] > 0.02f;
        if (hpfOn) hpf.setHighPass(20.f * std::pow(10.f, p[kHighPass]), 0.707f, fs); else hpf.setBypass();
        lpfOn = p[kLowPass] < 0.98f;
        if (lpfOn) lpf.setLowPass(2000.f * std::pow(10.f, p[kLowPass]), 0.707f, fs); else lpf.setBypass();

        master = p[kVolume] / 0.7f;
    }

    inline float process(float x) {
        // dual preamp blend — real 12AX7 (nodal triode) + solid-state (nodal op-amp)
        float s = (float)(tube.process((double)(tubeDrive * x)) + ss.process((double)(ssDrive * x))) * 0.6f;
        // compressor (downward, peak-following)
        if (compOn) {
            const float a = std::fabs(s);
            const float c = (a > env) ? atk : rel;
            env = c*env + (1.f-c)*a;
            if (env > compThr) s *= (compThr + (env-compThr)/compRatio) / (env + 1e-9f);
            s *= compMk;
        }
        // graphic EQ
        for (int i=0;i<kNumEq;++i) s = eq[i].process(s);
        // tone filters
        s = hpf.process(s);
        s = lpf.process(s);
        return s * master;
    }
};

class SharkePlugin : public Plugin {
    HartkeChannel L, R;
    float fParams[kParamCount];
    void recalc(){ L.setParams(fParams); R.setParams(fParams); }
public:
    SharkePlugin() : Plugin(kParamCount, 0, 0) {
        for (int i=0;i<kParamCount;++i) fParams[i]=kHartkeDef[i];
        const float sr=(float)getSampleRate();
        L.setSampleRate(sr); R.setSampleRate(sr); L.reset(); R.reset(); recalc();
    }
protected:
    const char* getLabel()       const override { return "SharkeHB3500"; }
    const char* getDescription() const override { return "Hartke HA3500 bass head model"; }
    const char* getMaker()       const override { return "RigBuilder"; }
    const char* getLicense()     const override { return "ISC"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 0); }
    int64_t     getUniqueId()    const override { return d_cconst('R', 'B', 'H', 'k'); }

    void initParameter(uint32_t i, Parameter& p) override {
        if (i >= (uint32_t)kParamCount) return;
        p.hints = kParameterIsAutomatable;
        if (i >= (uint32_t)kActive) p.hints |= kParameterIsBoolean;
        p.name = kHartkeNames[i]; p.symbol = kHartkeSymbols[i];
        p.ranges.min = kHartkeMin[i]; p.ranges.max = kHartkeMax[i]; p.ranges.def = kHartkeDef[i];
    }
    float getParameterValue(uint32_t i) const override { return (i < (uint32_t)kParamCount) ? fParams[i] : 0.f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < (uint32_t)kParamCount) { fParams[i]=v; recalc(); } }
    void  sampleRateChanged(double r) override { L.setSampleRate((float)r); R.setSampleRate((float)r); L.reset(); R.reset(); recalc(); }

    void run(const float** in, float** out, uint32_t frames) override {
        const float* iL=in[0]; const float* iR=in[1]; float* oL=out[0]; float* oR=out[1];
        for (uint32_t i=0;i<frames;++i){ oL[i]=L.process(iL[i]); oR[i]=R.process(iR[i]); }
    }
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharkePlugin)
};

Plugin* createPlugin() { return new SharkePlugin(); }

END_NAMESPACE_DISTRHO
