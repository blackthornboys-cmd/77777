#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <array>

namespace frozen {

enum class ScaleType {
    Chromatic = 0,
    Major,
    Minor,
    Dorian,
    PentatonicMajor,
    HarmonicMinor,
    Custom
};

enum class InputRange {
    Soprano = 0,
    AltoTenor,
    LowMale
};

struct PitchTelemetry {
    float detectedHz = 0.0f;
    float targetHz = 0.0f;
    float centsError = 0.0f;
    float appliedRatio = 1.0f;
    int detectedMidi = -1;
    int targetMidi = -1;
    float latencyMs = 0.0f;
    bool sibilantActive = false;
};

class FastFFT {
public:
    void prepare(int fftSize) {
        N = fftSize;
        logN = 0;
        while ((1 << logN) < N) ++logN;
        twiddles.resize(N / 2);
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < N / 2; ++i) {
            double ang = -2.0 * pi * i / N;
            twiddles[i] = std::complex<float>(static_cast<float>(std::cos(ang)),
                                              static_cast<float>(std::sin(ang)));
        }
        bitRev.resize(N);
        for (int i = 0; i < N; ++i) {
            int rev = 0;
            for (int b = 0; b < logN; ++b) {
                if (i & (1 << b)) rev |= (1 << (logN - 1 - b));
            }
            bitRev[i] = rev;
        }
    }

    void transform(std::vector<std::complex<float>>& buf, bool inverse) const {
        for (int i = 0; i < N; ++i) {
            int j = bitRev[i];
            if (i < j) std::swap(buf[i], buf[j]);
        }
        for (int len = 2; len <= N; len <<= 1) {
            int half = len >> 1;
            int step = N / len;
            for (int i = 0; i < N; i += len) {
                for (int k = 0; k < half; ++k) {
                    std::complex<float> w = twiddles[k * step];
                    if (inverse) w = std::conj(w);
                    auto u = buf[i + k];
                    auto v = buf[i + k + half] * w;
                    buf[i + k] = u + v;
                    buf[i + k + half] = u - v;
                }
            }
        }
        if (inverse) {
            float invN = 1.0f / static_cast<float>(N);
            for (int i = 0; i < N; ++i) buf[i] *= invN;
        }
    }

private:
    int N = 0;
    int logN = 0;
    std::vector<std::complex<float>> twiddles;
    std::vector<int> bitRev;
};

class PitchEngine {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) {
        fs = static_cast<float>(sampleRate > 0.0 ? sampleRate : 44100.0);

        // 4096-pt FFT guarantees zero circular aliasing for W=1024, B=2048 (1024+2048=3072 <= 4096)
        // and 2048-pt FFT for Low Latency W=640, B=1280 (640+1280=1920 <= 2048)
        fftLow.prepare(2048);
        fftHQ.prepare(4096);
        fftBufA.assign(4096, {0.0f, 0.0f});
        fftBufB.assign(4096, {0.0f, 0.0f});

        yinSize = 4096;
        yinRing.assign(yinSize, 0.0f);
        yinBuffer.assign(1024, 0.0f);
        cumEnergy.assign(2049, 0.0);
        yinWritePos = 0;
        samplesSinceAnalysis = 0;

        delaySize = 8192;
        delayMask = delaySize - 1;
        delayRing.assign(delaySize, 0.0f);
        doublerRingL.assign(4096, 0.0f);
        doublerRingR.assign(4096, 0.0f);
        delayWritePos = 0;
        doublerPos = 0;

        grainPhase = 0.0f;
        smoothedRatio = 1.0f;
        smoothedMix = 1.0f;
        smoothedDoublerWet = 0.0f;
        smoothedDoublerWidth = 0.5f;
        smoothedWindow = 720.0f;

        prevSample = 0.0f;
        hpState = 0.0f;
        hpPrevIn = 0.0f;
        zcrSmooth = 0.0f;
        stabilizedMidi = -1;
        candidateMidi = -1;
        candidateHoldSamples = 0;
        octaveCandidateHz = 0.0f;
        octaveHoldCount = 0;
        customMask.fill(true);
        resetTelemetry();
    }

    void setParameters(int rootNoteIndex, ScaleType scaleType, InputRange range,
                       float speedVal, float sustainPct, float amountPct,
                       float trackingPct, int stabilizerMode,
                       float doublerWetPct, float doublerWidthPct,
                       float pitchRefHz,
                       bool lowLatencyMode, bool sibilanceGuardOn, bool formantLockOn,
                       const std::array<bool, 12>& customNotes) {
        rootNote = std::clamp(rootNoteIndex, 0, 11);
        scale = scaleType;
        inputRange = range;
        retuneSpeed = std::clamp(speedVal, -5.0f, 150.0f);
        sustain = std::clamp(sustainPct, 0.0f, 100.0f) * 0.01f;
        targetMix = std::clamp(amountPct, 0.0f, 100.0f) * 0.01f;
        yinThreshold = 0.08f + (std::clamp(trackingPct, 0.0f, 100.0f) * 0.01f) * 0.16f;

        float stabMs = 0.0f;
        if (stabilizerMode == 1) stabMs = 40.0f;
        else if (stabilizerMode == 2) stabMs = 80.0f;
        else if (stabilizerMode == 3) stabMs = 150.0f;
        stabilizerHoldSamples = static_cast<int>((stabMs * 0.001f) * fs);

        targetDoublerWet = std::clamp(doublerWetPct, 0.0f, 100.0f) * 0.01f;
        targetDoublerWidth = std::clamp(doublerWidthPct, 0.0f, 100.0f) * 0.01f;
        referenceHz = std::clamp(pitchRefHz, 415.0f, 465.0f);
        lowLatency = lowLatencyMode;
        sibilanceGuard = sibilanceGuardOn;
        formantLock = formantLockOn;
        customMask = customNotes;
    }

    int getLatencySamples() const {
        return lowLatency ? 0 : 128;
    }

    float getLatencyMs() const {
        return (static_cast<float>(getLatencySamples()) / fs) * 1000.0f;
    }

    void processStereoSample(float inL, float inR, float& outL, float& outR) {
        float monoIn = 0.5f * (inL + inR);

        constexpr float pCoeff = 0.002f;
        smoothedMix += pCoeff * (targetMix - smoothedMix);
        smoothedDoublerWet += pCoeff * (targetDoublerWet - smoothedDoublerWet);
        smoothedDoublerWidth += pCoeff * (targetDoublerWidth - smoothedDoublerWidth);

        // Sibilance / Consonant detector
        bool signChanged = (monoIn >= 0.0f && prevSample < 0.0f) || (monoIn < 0.0f && prevSample >= 0.0f);
        prevSample = monoIn;
        zcrSmooth += 0.015f * ((signChanged ? 1.0f : 0.0f) - zcrSmooth);
        isSibilant = sibilanceGuard && (zcrSmooth > 0.36f);

        // 75 Hz HPF on YIN analysis branch only
        hpState = 0.989f * (hpState + monoIn - hpPrevIn);
        hpPrevIn = monoIn;
        yinRing[yinWritePos] = hpState;
        yinWritePos = (yinWritePos + 1) % yinSize;

        const int analysisHop = lowLatency ? 64 : 128;
        if (++samplesSinceAnalysis >= analysisHop) {
            samplesSinceAnalysis = 0;
            if (!isSibilant) {
                runLinearYin(analysisHop);
            }
        }

        float targetRatio = 1.0f;
        if (!isSibilant && currentDetectedHz > 55.0f && currentTargetHz > 55.0f) {
            float rawRatio = currentTargetHz / currentDetectedHz;
            if (sustain > 0.01f && std::abs(currentCentsError) < 35.0f) {
                rawRatio = 1.0f + (rawRatio - 1.0f) * (1.0f + sustain * 0.12f);
            }
            targetRatio = std::clamp(rawRatio, 0.5f, 2.0f);
        }

        // Smooth pitch ratio (never jump abruptly on a single sample!)
        float effMs = (retuneSpeed < 0.0f) ? (0.4f + (retuneSpeed + 5.0f) * 0.12f) : std::max(1.0f, retuneSpeed);
        float tauSec = effMs * 0.001f;
        float coeff = 1.0f - std::exp(-1.0f / (tauSec * fs));
        smoothedRatio += coeff * (targetRatio - smoothedRatio);

        // Smoothly adapt grain window to pitch period in Modern mode (prevents warble & preserves tone)
        float targetWin = (formantLock && currentDetectedHz > 65.0f)
            ? std::clamp((fs / currentDetectedHz) * 3.0f, 420.0f, 1100.0f)
            : (lowLatency ? 640.0f : 960.0f);
        smoothedWindow += 0.002f * (targetWin - smoothedWindow);

        delayRing[delayWritePos & delayMask] = monoIn;

        float minOffset = 4.0f;
        float phasorStep = (1.0f - smoothedRatio) / smoothedWindow;
        grainPhase += phasorStep;
        if (grainPhase >= 1.0f) grainPhase -= 1.0f;
        if (grainPhase < 0.0f)  grainPhase += 1.0f;

        float phaseA = grainPhase;
        float phaseB = grainPhase + 0.5f;
        if (phaseB >= 1.0f) phaseB -= 1.0f;

        float tapA = readCubic(phaseA * smoothedWindow + minOffset);
        float tapB = readCubic(phaseB * smoothedWindow + minOffset);

        constexpr float pi = 3.14159265358979323846f;
        float winA = 0.5f * (1.0f - std::cos(2.0f * pi * phaseA));
        float winB = 0.5f * (1.0f - std::cos(2.0f * pi * phaseB));

        float wet = tapA * winA + tapB * winB;
        delayWritePos++;

        float tuned = monoIn * (1.0f - smoothedMix) + wet * smoothedMix;

        // 2D Stereo Doubler
        doublerRingL[doublerPos & 4095] = tuned;
        doublerRingR[doublerPos & 4095] = tuned;

        if (smoothedDoublerWet > 0.005f) {
            doublerLfo += 0.00018f;
            if (doublerLfo > 2.0f * pi) doublerLfo -= 2.0f * pi;
            float spreadMs = 4.0f + smoothedDoublerWidth * 16.0f;
            float modL = spreadMs * (fs * 0.001f) + std::sin(doublerLfo) * (1.2f * fs * 0.001f);
            float modR = (spreadMs * 1.45f) * (fs * 0.001f) + std::cos(doublerLfo * 0.87f) * (1.4f * fs * 0.001f);

            auto readDoubler = [&](const std::vector<float>& ring, float d) {
                float rp = static_cast<float>(doublerPos) - d;
                int i0 = static_cast<int>(std::floor(rp));
                float f = rp - static_cast<float>(i0);
                return ring[i0 & 4095] + f * (ring[(i0 + 1) & 4095] - ring[i0 & 4095]);
            };

            float dblL = readDoubler(doublerRingL, modL);
            float dblR = readDoubler(doublerRingR, modR);
            float dryGain = 1.0f - smoothedDoublerWet * 0.25f;
            outL = tuned * dryGain + dblL * smoothedDoublerWet * 0.45f;
            outR = tuned * dryGain + dblR * smoothedDoublerWet * 0.45f;
        } else {
            outL = tuned;
            outR = tuned;
        }
        doublerPos++;
    }

    PitchTelemetry getTelemetry() const {
        return {
            currentDetectedHz,
            currentTargetHz,
            currentCentsError,
            smoothedRatio,
            currentDetectedMidi,
            currentTargetMidi,
            getLatencyMs(),
            isSibilant
        };
    }

    static bool isNoteInScale(int midiNote, int root, ScaleType s, const std::array<bool, 12>& custom) {
        int rawPc = ((midiNote % 12) + 12) % 12;
        if (s == ScaleType::Custom) return custom[static_cast<size_t>(rawPc)];
        int pc = (rawPc - root + 12) % 12;
        switch (s) {
            case ScaleType::Chromatic:       return true;
            case ScaleType::Major:           return pc==0 || pc==2 || pc==4 || pc==5 || pc==7 || pc==9 || pc==11;
            case ScaleType::Minor:           return pc==0 || pc==2 || pc==3 || pc==5 || pc==7 || pc==8 || pc==10;
            case ScaleType::Dorian:          return pc==0 || pc==2 || pc==3 || pc==5 || pc==7 || pc==9 || pc==10;
            case ScaleType::PentatonicMajor: return pc==0 || pc==2 || pc==4 || pc==7 || pc==9;
            case ScaleType::HarmonicMinor:   return pc==0 || pc==2 || pc==3 || pc==5 || pc==7 || pc==8 || pc==11;
            case ScaleType::Custom:          return custom[static_cast<size_t>(rawPc)];
        }
        return true;
    }

    static int quantizeMidiNote(float continuousMidi, int root, ScaleType s, const std::array<bool, 12>& custom) {
        int base = static_cast<int>(std::round(continuousMidi));
        if (isNoteInScale(base, root, s, custom)) return base;

        int bestNote = base;
        float bestDist = 100.0f;
        for (int offset = -6; offset <= 6; ++offset) {
            int candidate = base + offset;
            if (isNoteInScale(candidate, root, s, custom)) {
                float d = std::abs(continuousMidi - static_cast<float>(candidate));
                if (d < bestDist) {
                    bestDist = d;
                    bestNote = candidate;
                }
            }
        }
        return bestNote;
    }

private:
    void resetTelemetry() {
        currentDetectedHz = 0.0f;
        currentTargetHz = 0.0f;
        currentCentsError = 0.0f;
        currentDetectedMidi = -1;
        currentTargetMidi = -1;
    }

    float readCubic(float delaySamples) const {
        float readPos = static_cast<float>(delayWritePos) - delaySamples;
        int idx1 = static_cast<int>(std::floor(readPos));
        float f = readPos - static_cast<float>(idx1);
        float y0 = delayRing[(idx1 - 1) & delayMask];
        float y1 = delayRing[idx1 & delayMask];
        float y2 = delayRing[(idx1 + 1) & delayMask];
        float y3 = delayRing[(idx1 + 2) & delayMask];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    void runLinearYin(int hopSamples) {
        const int W = lowLatency ? 640 : 1024;
        const int activeFftSize = lowLatency ? 2048 : 4096;
        const auto& activeFft = lowLatency ? fftLow : fftHQ;
        const int baseOffset = yinWritePos + yinSize - W * 2;

        cumEnergy[0] = 0.0;
        for (int i = 0; i < W * 2; ++i) {
            float s = yinRing[(baseOffset + i) % yinSize];
            cumEnergy[i + 1] = cumEnergy[i] + static_cast<double>(s) * s;
        }

        double frameEnergy = cumEnergy[W];
        if (frameEnergy / W < 1e-5) {
            resetTelemetry();
            stabilizedMidi = -1;
            octaveHoldCount = 0;
            return;
        }

        // Zero-padded linear cross-correlation (zero circular aliasing!)
        std::fill(fftBufA.begin(), fftBufA.begin() + activeFftSize, std::complex<float>(0.0f, 0.0f));
        std::fill(fftBufB.begin(), fftBufB.begin() + activeFftSize, std::complex<float>(0.0f, 0.0f));

        for (int i = 0; i < W; ++i) {
            fftBufA[i] = std::complex<float>(yinRing[(baseOffset + i) % yinSize], 0.0f);
        }
        for (int i = 0; i < W * 2; ++i) {
            fftBufB[i] = std::complex<float>(yinRing[(baseOffset + i) % yinSize], 0.0f);
        }

        activeFft.transform(fftBufA, false);
        activeFft.transform(fftBufB, false);
        for (int i = 0; i < activeFftSize; ++i) {
            fftBufA[i] = std::conj(fftBufA[i]) * fftBufB[i];
        }
        activeFft.transform(fftBufA, true);

        float minHz = 95.0f, maxHz = 800.0f;
        if (inputRange == InputRange::Soprano) { minHz = 140.0f; maxHz = 1100.0f; }
        else if (inputRange == InputRange::LowMale) { minHz = 65.0f; maxHz = 500.0f; }

        int minTau = std::max(2, static_cast<int>(fs / maxHz));
        int maxTau = std::min(W - 2, static_cast<int>(fs / minHz));
        if (maxTau <= minTau) return;

        yinBuffer[0] = 1.0f;
        double runningSum = 0.0;
        for (int tau = 1; tau <= maxTau; ++tau) {
            double e2 = cumEnergy[tau + W] - cumEnergy[tau];
            double diff = std::max(0.0, frameEnergy + e2 - 2.0 * static_cast<double>(fftBufA[tau].real()));
            runningSum += diff;
            yinBuffer[tau] = (runningSum > 1e-12)
                ? static_cast<float>(diff * tau / runningSum)
                : 1.0f;
        }

        int tauEstimate = -1;
        for (int tau = minTau; tau < maxTau; ++tau) {
            if (yinBuffer[tau] < yinThreshold) {
                while (tau + 1 < maxTau && yinBuffer[tau + 1] < yinBuffer[tau]) ++tau;
                tauEstimate = tau;
                break;
            }
        }
        if (tauEstimate < 0) return;

        float betterTau = static_cast<float>(tauEstimate);
        if (tauEstimate > 0 && tauEstimate + 1 <= maxTau) {
            float s0 = yinBuffer[tauEstimate - 1];
            float s1 = yinBuffer[tauEstimate];
            float s2 = yinBuffer[tauEstimate + 1];
            float denom = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::abs(denom) > 1e-6f) betterTau += (s2 - s0) / denom;
        }

        float hz = fs / std::max(1.0f, betterTau);
        if (hz < minHz || hz > maxHz) return;

        // Octave-jump glitch filter: reject sudden >7 semitone jumps unless sustained 2 frames
        if (currentDetectedHz > 50.0f) {
            float semitoneJump = std::abs(12.0f * std::log2(hz / currentDetectedHz));
            if (semitoneJump > 7.0f) {
                if (std::abs(hz - octaveCandidateHz) < hz * 0.05f) {
                    if (++octaveHoldCount < 2) return;
                } else {
                    octaveCandidateHz = hz;
                    octaveHoldCount = 1;
                    return;
                }
            }
        }
        octaveHoldCount = 0;

        currentDetectedHz = hz;
        float midiFloat = 69.0f + 12.0f * std::log2(hz / referenceHz);
        currentDetectedMidi = static_cast<int>(std::round(midiFloat));

        int rawTarget = quantizeMidiNote(midiFloat, rootNote, scale, customMask);

        if (stabilizerHoldSamples > 0 && stabilizedMidi >= 0 && rawTarget != stabilizedMidi) {
            if (rawTarget == candidateMidi) {
                candidateHoldSamples += hopSamples;
                if (candidateHoldSamples >= stabilizerHoldSamples) {
                    stabilizedMidi = rawTarget;
                    candidateHoldSamples = 0;
                }
            } else {
                candidateMidi = rawTarget;
                candidateHoldSamples = hopSamples;
            }
        } else {
            stabilizedMidi = rawTarget;
            candidateMidi = rawTarget;
            candidateHoldSamples = 0;
        }

        currentTargetMidi = stabilizedMidi;
        currentTargetHz = referenceHz * std::pow(2.0f, (static_cast<float>(currentTargetMidi) - 69.0f) / 12.0f);
        currentCentsError = (midiFloat - static_cast<float>(currentTargetMidi)) * 100.0f;
    }

    float fs = 44100.0f;
    int rootNote = 0;
    ScaleType scale = ScaleType::Chromatic;
    InputRange inputRange = InputRange::AltoTenor;
    float retuneSpeed = 12.0f;
    float sustain = 0.0f;
    float targetMix = 1.0f;
    float smoothedMix = 1.0f;
    float yinThreshold = 0.16f;
    int stabilizerHoldSamples = 0;
    float targetDoublerWet = 0.0f;
    float targetDoublerWidth = 0.5f;
    float smoothedDoublerWet = 0.0f;
    float smoothedDoublerWidth = 0.5f;
    float referenceHz = 440.0f;
    bool lowLatency = true;
    bool sibilanceGuard = true;
    bool formantLock = true;
    std::array<bool, 12> customMask{};

    int yinSize = 4096;
    FastFFT fftLow, fftHQ;
    std::vector<std::complex<float>> fftBufA, fftBufB;
    std::vector<float> yinRing;
    std::vector<float> yinBuffer;
    std::vector<double> cumEnergy;
    int yinWritePos = 0;
    int samplesSinceAnalysis = 0;

    int delaySize = 8192;
    int delayMask = 8191;
    std::vector<float> delayRing;
    std::vector<float> doublerRingL, doublerRingR;
    int delayWritePos = 0;
    int doublerPos = 0;
    float doublerLfo = 0.0f;
    float grainPhase = 0.0f;
    float smoothedRatio = 1.0f;
    float smoothedWindow = 720.0f;

    float prevSample = 0.0f;
    float hpState = 0.0f;
    float hpPrevIn = 0.0f;
    float zcrSmooth = 0.0f;
    bool isSibilant = false;

    int stabilizedMidi = -1;
    int candidateMidi = -1;
    int candidateHoldSamples = 0;
    float octaveCandidateHz = 0.0f;
    int octaveHoldCount = 0;

    float currentDetectedHz = 0.0f;
    float currentTargetHz = 0.0f;
    float currentCentsError = 0.0f;
    int currentDetectedMidi = -1;
    int currentTargetMidi = -1;
};

} // namespace frozen
