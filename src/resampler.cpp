#include "resampler.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
double linear_interp(double first_number, double second_number, double fraction) {
    return (first_number + ((second_number - first_number)*fraction));
}
}

Resampler::Resampler() :
    m_sincTable(kWidth*kSamplesPerZeroCrossing),
    m_delayBuffer(3*kWidth),
    m_edgeSamples(kWidth),
    m_samplesInp(2048) {
    make_sinc();
    reset();
}

void Resampler::reset() {
    m_state = {};
    std::fill(m_edgeSamples.begin(), m_edgeSamples.end(), 0.0f);
    std::fill(m_delayBuffer.begin(), m_delayBuffer.end(), 0.0f);
    std::fill(m_samplesInp.begin(), m_samplesInp.end(), 0.0f);
}

int Resampler::resample(
        float factor,
        int nSamples,
        const float * samplesInp,
        float * samplesOut) {
    int idxInp = -1;
    int idxOut = 0;
    int notDone = 1;
    float data_in = 0.0f;
    float data_out = 0.0f;
    double one_over_factor = 1.0;

    auto stateSave = m_state;

    m_state.nSamplesTotal += nSamples;

    if (samplesOut) {
        assert(nSamples > kWidth);
        if ((int) m_samplesInp.size() < nSamples + kWidth) {
            m_samplesInp.resize(nSamples + kWidth);
        }
        for (int i = 0; i < kWidth; ++i) {
            m_samplesInp[i] = m_edgeSamples[i];
            m_edgeSamples[i] = samplesInp[nSamples - kWidth + i];
        }
        for (int i = 0; i < nSamples; ++i) {
            m_samplesInp[i + kWidth] = samplesInp[i];
        }
        samplesInp = m_samplesInp.data();
    }

    while (notDone) {
        while (m_state.timeLast < m_state.timeInt) {
            if (++idxInp >= nSamples) {
                notDone = 0;
                break;
            } else {
                data_in = samplesInp[idxInp];
            }
            //printf("xxxx idxInp = %d\n", idxInp);
            if (samplesOut) new_data(data_in);
            m_state.timeLast += 1;
        }

        if (notDone == false) break;

        double temp1 = 0.0;
        int left_limit = m_state.timeNow - kWidth + 1; /* leftmost neighboring sample used for interp.*/
        int right_limit = m_state.timeNow + kWidth;    /* rightmost leftmost neighboring sample used for interp.*/
        if (left_limit < 0) left_limit = 0;
        if (right_limit > m_state.nSamplesTotal + kWidth) right_limit = m_state.nSamplesTotal + kWidth;
        if (factor < 1.0) {
            for (int j = left_limit; j < right_limit; j++) {
                temp1 += gimme_data(j - m_state.timeInt)*sinc(m_state.timeNow - (double) j);
            }
            data_out = temp1;
        }
        else {
            one_over_factor = 1.0 / factor;
            for (int j = left_limit; j < right_limit; j++) {
                temp1 += gimme_data(j - m_state.timeInt)*one_over_factor*sinc(one_over_factor*(m_state.timeNow - (double) j));
            }
            data_out = temp1;
        }

        if (samplesOut) {
            //printf("inp = %d, l = %d, r = %d, n = %d, a = %d, b = %d\n", idxInp, left_limit, right_limit, m_state.nSamplesTotal, left_limit - m_state.timeInt, right_limit - m_state.timeInt - 1);
            samplesOut[idxOut] = data_out;
        }
        ++idxOut;

        m_state.timeNow += factor;
        m_state.timeLast = m_state.timeInt;
        m_state.timeInt = m_state.timeNow;
        while (m_state.timeLast < m_state.timeInt) {
            if (++idxInp >= nSamples) {
                notDone = 0;
                break;
            } else {
                data_in = samplesInp[idxInp];
            }
            if (samplesOut) new_data(data_in);
            m_state.timeLast += 1;
        }
        //printf("last idxInp = %d, nSamples = %d\n", idxInp, nSamples);
    }

    if (samplesOut == nullptr) {
        m_state = stateSave;
    }

    return idxOut;
}

float Resampler::gimme_data(int j) const {
    return m_delayBuffer[(int) j + kWidth];
}

void Resampler::new_data(float data) {
    for (int i = 0; i < kDelaySize - 5; i++) {
        m_delayBuffer[i] = m_delayBuffer[i + 1];
    }
    m_delayBuffer[kDelaySize - 5] = data;
}

void Resampler::make_sinc() {
    double temp, win_freq, win;
    win_freq = M_PI/kWidth/kSamplesPerZeroCrossing;
    m_sincTable[0] = 1.0;
    for (int i = 1; i < kWidth*kSamplesPerZeroCrossing; i++) {
        temp = (double) i*M_PI/kSamplesPerZeroCrossing;
        m_sincTable[i] = sin(temp)/temp;
        win = 0.5 + 0.5*cos(win_freq*i);
        m_sincTable[i] *= win;
    }
}

double Resampler::sinc(double x) const {
    int low;
    double temp, delta;
    if (fabs(x) >= kWidth - 1) {
        return 0.0;
    } else {
        temp = fabs(x)*(double) kSamplesPerZeroCrossing;
        low = temp;          /* these are interpolation steps */
        delta = temp - low;  /* and can be ommited if desired */
        return linear_interp(m_sincTable[low], m_sincTable[low + 1], delta);
    }
}
