#include "resampler.h"

#include <cmath>
#include <cstdio>

namespace {
double linear_interp(double first_number, double second_number, double fraction) {
    return (first_number + ((second_number - first_number)*fraction));
}
}

int Resampler::resample(
        float factor,
        int nSamples,
        const float * samplesInp,
        float * samplesOut) {
    if (factor != m_lastFactor) {
        make_sinc();
        m_lastFactor = factor;
    }

    int idxInp = 0;
    int idxOut = 0;
    int notDone = 1;
    double time_now = 0.0;
    long num_samples = nSamples;
    long int_time = 0;
    long last_time = 0;
    float data_in = samplesInp[idxInp];
    float data_out;
    double one_over_factor = 1.0;
    while (notDone) {
        double temp1 = 0.0;
        long left_limit = time_now - kWidth + 1;      /* leftmost neighboring sample used for interp.*/
        long right_limit = time_now + kWidth; /* rightmost leftmost neighboring sample used for interp.*/
        if (left_limit<0) left_limit = 0;
        if (right_limit>num_samples) right_limit = num_samples;
        if (factor<1.0) {
            for (int j=left_limit;j<right_limit;j++) {
                temp1 += gimme_data(j-int_time)*sinc(time_now - (double) j);
            }
            data_out = temp1;
        }
        else {
            one_over_factor = 1.0 / factor;
            for (int j=left_limit;j<right_limit;j++) {
                temp1 += gimme_data(j-int_time)*one_over_factor*sinc(one_over_factor * (time_now - (double) j));
            }
            data_out = temp1;
        }

        //printf("%8.8f %8.8f\n", data_in, data_out);
        samplesOut[idxOut++] = data_out;
        time_now += factor;
        last_time = int_time;
        int_time = time_now;
        while(last_time<int_time)      {
            if (++idxInp == nSamples) {
                notDone = 0;
            } else {
                data_in = samplesInp[idxInp];
            }
            new_data(data_in);
            last_time += 1;
        }
        //                if (!(int_time % 1000)) printf("Sample # %li\n",int_time);
        //if (!(int_time % 1000)) {
        //    printf(".");
        //    fflush(stdout);
        //}
    }

    return idxOut;
}

float Resampler::gimme_data(long j) const {
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
    for (int i = 1; i < kWidth*kSamplesPerZeroCrossing;i++) {
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
        temp = fabs(x) * (double) kSamplesPerZeroCrossing;
        low = temp;          /* these are interpolation steps */
        delta = temp - low;  /* and can be ommited if desired */
        return linear_interp(m_sincTable[low], m_sincTable[low + 1], delta);
    }
}
