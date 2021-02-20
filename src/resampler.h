#pragma once

class Resampler {
public:
    int resample(
            float factor,
            int nSamples,
            const float * samplesInp,
            float * samplesOut);
private:
    float gimme_data(long j) const;
    void new_data(float data);
    void make_sinc();
    double sinc(double x) const;

    /* this controls the number of neighboring samples
       which are used to interpolate the new samples.  The
       processing time is linearly related to this width */
    static const int kWidth = 64;

    static const int kDelaySize = 140;

    /* this defines how finely the sinc function
       is sampled for storage in the table  */
    static const int kSamplesPerZeroCrossing = 32;

    float m_sincTable[kWidth*kSamplesPerZeroCrossing] = { 0.0 };

    float m_delayBuffer[3*kWidth] = { 0 };

    float m_lastFactor = -1.0f;
};
