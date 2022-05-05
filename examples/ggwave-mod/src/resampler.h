#pragma once

#include <vector>
#include <cstdint>

class Resampler {
public:
    // this controls the number of neighboring samples
    // which are used to interpolate the new samples. The
    // processing time is linearly related to this width
    static const int kWidth = 64;

    Resampler();

    void reset();

    int nSamplesTotal() const { return m_state.nSamplesTotal; }

    int resample(
            float factor,
            int nSamples,
            const float * samplesInp,
            float * samplesOut);

private:
    float gimme_data(int j) const;
    void new_data(float data);
    void make_sinc();
    double sinc(double x) const;

    static const int kDelaySize = 140;

    // this defines how finely the sinc function is sampled for storage in the table
    static const int kSamplesPerZeroCrossing = 32;

    std::vector<float> m_sincTable;
    std::vector<float> m_delayBuffer;
    std::vector<float> m_edgeSamples;
    std::vector<float> m_samplesInp;

    struct State {
        int nSamplesTotal = 0;
        int timeInt = 0;
        int timeLast = 0;
        double timeNow = 0.0;
    };

    State m_state;
};
