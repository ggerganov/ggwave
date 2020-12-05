#include "ggwave/ggwave.h"

#include "reed-solomon/rs.hpp"

#include <chrono>
#include <algorithm>
#include <random>

namespace {

// FFT routines taken from https://stackoverflow.com/a/37729648/4039976

int log2(int N) {
    int k = N, i = 0;
    while(k) {
        k >>= 1;
        i++;
    }
    return i - 1;
}

int reverse(int N, int n) {
    int j, p = 0;
    for(j = 1; j <= log2(N); j++) {
        if(n & (1 << (log2(N) - j)))
            p |= 1 << (j - 1);
    }
    return p;
}

void ordina(std::complex<float>* f1, int N) {
    std::complex<float> f2[GGWave::kMaxSamplesPerFrame];
    for(int i = 0; i < N; i++)
        f2[i] = f1[reverse(N, i)];
    for(int j = 0; j < N; j++)
        f1[j] = f2[j];
}

void transform(std::complex<float>* f, int N) {
    ordina(f, N);    //first: reverse order
    std::complex<float> *W;
    W = (std::complex<float> *)malloc(N / 2 * sizeof(std::complex<float>));
    W[1] = std::polar(1., -2. * M_PI / N);
    W[0] = 1;
    for(int i = 2; i < N / 2; i++)
        W[i] = pow(W[1], i);
    int n = 1;
    int a = N / 2;
    for(int j = 0; j < log2(N); j++) {
        for(int i = 0; i < N; i++) {
            if(!(i & n)) {
                std::complex<float> temp = f[i];
                std::complex<float> Temp = W[(i * a) % (n * a)] * f[i + n];
                f[i] = temp + Temp;
                f[i + n] = temp - Temp;
            }
        }
        n *= 2;
        a = a / 2;
    }
    free(W);
}

void FFT(std::complex<float>* f, int N, float d) {
    transform(f, N);
    for(int i = 0; i < N; i++)
        f[i] *= d; //multiplying by step
}

void FFT(float * src, std::complex<float>* dst, int N, float d) {
    for (int i = 0; i < N; ++i) {
        dst[i].real(src[i]);
        dst[i].imag(0);
    }
    FFT(dst, N, d);
}

inline void addAmplitudeSmooth(
        const GGWave::AmplitudeData & src,
        GGWave::AmplitudeData & dst,
        float scalar, int startId, int finalId, int cycleMod, int nPerCycle) {
    int nTotal = nPerCycle*finalId;
    float frac = 0.15f;
    float ds = frac*nTotal;
    float ids = 1.0f/ds;
    int nBegin = frac*nTotal;
    int nEnd = (1.0f - frac)*nTotal;
    for (int i = startId; i < finalId; i++) {
        float k = cycleMod*finalId + i;
        if (k < nBegin) {
            dst[i] += scalar*src[i]*(k*ids);
        } else if (k > nEnd) {
            dst[i] += scalar*src[i]*(((float)(nTotal) - k)*ids);
        } else {
            dst[i] += scalar*src[i];
        }
    }
}

template <class T>
float getTime_ms(const T & tStart, const T & tEnd) {
    return ((float)(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count()))/1000.0;
}

int getECCBytesForLength(int len) {
    return std::max(4, 2*(len/5));
}

}

GGWave::GGWave(
        int sampleRateIn,
        int sampleRateOut,
        int samplesPerFrame,
        int sampleSizeBytesIn,
        int sampleSizeBytesOut) :
    m_sampleRateIn(sampleRateIn),
    m_sampleRateOut(sampleRateOut),
    m_samplesPerFrame(samplesPerFrame),
    m_isamplesPerFrame(1.0f/m_samplesPerFrame),
    m_sampleSizeBytesIn(sampleSizeBytesIn),
    m_sampleSizeBytesOut(sampleSizeBytesOut),
    m_hzPerSample(m_sampleRateIn/samplesPerFrame),
    m_ihzPerSample(1.0f/m_hzPerSample),
    m_freqDelta_bin(1),
    m_freqDelta_hz(2*m_hzPerSample),
    m_nBitsInMarker(16),
    m_nMarkerFrames(16),
    m_nPostMarkerFrames(0),
    m_encodedDataOffset(3),
    m_rsLength(new RS::ReedSolomon(1, m_encodedDataOffset - 1))
{
    init(0, "", getDefultTxProtocol(), 0);
}

GGWave::~GGWave() {
}

bool GGWave::init(int textLength, const char * stext, const TxProtocol & aProtocol, const int volume) {
    if (textLength > kMaxLength) {
        printf("Truncating data from %d to 140 bytes\n", textLength);
        textLength = kMaxLength;
    }

    m_txProtocol = aProtocol;
    m_txDataLength = textLength;
    m_sendVolume = ((double)(volume))/100.0f;

    const uint8_t * text = reinterpret_cast<const uint8_t *>(stext);

    m_hasNewTxData = false;
    m_txData.fill(0);
    m_txDataEncoded.fill(0);

    if (m_txDataLength > 0) {
        m_txData[0] = m_txDataLength;
        for (int i = 0; i < m_txDataLength; ++i) m_txData[i + 1] = text[i];

        m_hasNewTxData = true;
    }

    // Rx
    m_receivingData = false;
    m_analyzingData = false;

    m_framesToAnalyze = 0;
    m_framesLeftToAnalyze = 0;
    m_framesToRecord = 0;
    m_framesLeftToRecord = 0;

    m_sampleAmplitude.fill(0);
    m_sampleSpectrum.fill(0);
    for (auto & s : m_sampleAmplitudeHistory) {
        s.fill(0);
    }

    m_rxData.fill(0);

    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_fftOut[i].real(0.0f);
        m_fftOut[i].imag(0.0f);
    }

    return true;
}

void GGWave::send(const CBQueueAudio & cbQueueAudio) {
    int samplesPerFrameOut = (m_sampleRateOut/m_sampleRateIn)*m_samplesPerFrame;
    if (m_sampleRateOut != m_sampleRateIn) {
        printf("Resampling from %d Hz to %d Hz\n", (int) m_sampleRateIn, (int) m_sampleRateOut);
    }

    int frameId = 0;

    AmplitudeData outputBlock;
    AmplitudeData16 outputBlock16;

    std::array<double, kMaxDataBits> phaseOffsets;

    for (int k = 0; k < (int) phaseOffsets.size(); ++k) {
        phaseOffsets[k] = (M_PI*k)/(m_txProtocol.nDataBitsPerTx());
    }

    // note : what is the purpose of this shuffle ? I forgot .. :(
    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(phaseOffsets.begin(), phaseOffsets.end(), g);

    std::array<bool, kMaxDataBits> dataBits;

    std::array<AmplitudeData, kMaxDataBits> bit1Amplitude;
    std::array<AmplitudeData, kMaxDataBits> bit0Amplitude;

    for (int k = 0; k < (int) dataBits.size(); ++k) {
        double freq = bitFreq(m_txProtocol, k);

        double phaseOffset = phaseOffsets[k];
        double curHzPerSample = m_sampleRateOut/m_samplesPerFrame;
        double curIHzPerSample = 1.0/curHzPerSample;
        for (int i = 0; i < m_samplesPerFrame; i++) {
            double curi = i;
            bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*(freq*curIHzPerSample) + phaseOffset);
        }
        for (int i = 0; i < m_samplesPerFrame; i++) {
            double curi = i;
            bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*((freq + m_hzPerSample*m_freqDelta_bin)*curIHzPerSample) + phaseOffset);
        }
    }

    m_nECCBytesPerTx = getECCBytesForLength(m_txDataLength);
    m_sendDataLength = m_txDataLength + m_encodedDataOffset;

    RS::ReedSolomon rsData = RS::ReedSolomon(m_txDataLength, m_nECCBytesPerTx);

    m_rsLength->Encode(m_txData.data(), m_txDataEncoded.data());
    rsData.Encode(m_txData.data() + 1, m_txDataEncoded.data() + m_encodedDataOffset);

    while (m_hasNewTxData) {
        std::fill(outputBlock.begin(), outputBlock.end(), 0.0f);

        if (m_sampleRateOut != m_sampleRateIn) {
            for (int k = 0; k < m_txProtocol.nDataBitsPerTx(); ++k) {
                double freq = bitFreq(m_txProtocol, k);

                double phaseOffset = phaseOffsets[k];
                double curHzPerSample = m_sampleRateOut/m_samplesPerFrame;
                double curIHzPerSample = 1.0/curHzPerSample;
                for (int i = 0; i < samplesPerFrameOut; i++) {
                    double curi = (i + frameId*samplesPerFrameOut);
                    bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*(freq*curIHzPerSample) + phaseOffset);
                }
                for (int i = 0; i < samplesPerFrameOut; i++) {
                    double curi = (i + frameId*samplesPerFrameOut);
                    bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*m_isamplesPerFrame)*((freq + m_hzPerSample*m_freqDelta_bin)*curIHzPerSample) + phaseOffset);
                }
            }
        }

        std::uint16_t nFreq = 0;
        if (frameId < m_nMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId, m_nMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId, m_nMarkerFrames);
                }
            }
        } else if (frameId < m_nMarkerFrames + m_nPostMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId - m_nMarkerFrames, m_nPostMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId - m_nMarkerFrames, m_nPostMarkerFrames);
                }
            }
        } else if (frameId <
                   (m_nMarkerFrames + m_nPostMarkerFrames) +
                   ((m_sendDataLength + m_nECCBytesPerTx)/m_txProtocol.bytesPerTx + 2)*m_txProtocol.framesPerTx) {
            int dataOffset = frameId - m_nMarkerFrames - m_nPostMarkerFrames;
            int cycleModMain = dataOffset%m_txProtocol.framesPerTx;
            dataOffset /= m_txProtocol.framesPerTx;
            dataOffset *= m_txProtocol.bytesPerTx;

            dataBits.fill(0);

            for (int j = 0; j < m_txProtocol.bytesPerTx; ++j) {
                {
                    uint8_t d = m_txDataEncoded[dataOffset + j] & 15;
                    dataBits[(2*j + 0)*16 + d] = 1;
                }
                {
                    uint8_t d = m_txDataEncoded[dataOffset + j] & 240;
                    dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                }
            }

            for (int k = 0; k < 2*m_txProtocol.bytesPerTx*16; ++k) {
                if (dataBits[k] == 0) continue;

                ++nFreq;
                if (k%2) {
                    ::addAmplitudeSmooth(bit0Amplitude[k/2], outputBlock, m_sendVolume, 0, samplesPerFrameOut, cycleModMain, m_txProtocol.framesPerTx);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[k/2], outputBlock, m_sendVolume, 0, samplesPerFrameOut, cycleModMain, m_txProtocol.framesPerTx);
                }
            }
        } else if (frameId <
                   (m_nMarkerFrames + m_nPostMarkerFrames) +
                   ((m_sendDataLength + m_nECCBytesPerTx)/m_txProtocol.bytesPerTx + 2)*m_txProtocol.framesPerTx +
                   (m_nMarkerFrames)) {
            nFreq = m_nBitsInMarker;

            int fId = frameId - ((m_nMarkerFrames + m_nPostMarkerFrames) + ((m_sendDataLength + m_nECCBytesPerTx)/m_txProtocol.bytesPerTx + 2)*m_txProtocol.framesPerTx);
            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    addAmplitudeSmooth(bit0Amplitude[i], outputBlock, m_sendVolume, 0, samplesPerFrameOut, fId, m_nMarkerFrames);
                } else {
                    addAmplitudeSmooth(bit1Amplitude[i], outputBlock, m_sendVolume, 0, samplesPerFrameOut, fId, m_nMarkerFrames);
                }
            }
        } else {
            m_hasNewTxData = false;
        }

        if (nFreq == 0) nFreq = 1;
        float scale = 1.0f/nFreq;
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            outputBlock[i] *= scale;
        }

        // todo : support for non-int16 output
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            outputBlock16[frameId*samplesPerFrameOut + i] = std::round(32000.0*outputBlock[i]);
        }

        ++frameId;
    }

    cbQueueAudio(outputBlock16.data(), frameId*samplesPerFrameOut*m_sampleSizeBytesOut);
}

void GGWave::receive(const CBDequeueAudio & CBDequeueAudio) {
    while (m_hasNewTxData == false) {
        // read capture data
        //
        // todo : support for non-float input
        auto nBytesRecorded = CBDequeueAudio(m_sampleAmplitude.data(), m_samplesPerFrame*m_sampleSizeBytesIn);

        if (nBytesRecorded != 0) {
            {
                m_sampleAmplitudeHistory[m_historyId] = m_sampleAmplitude;

                if (++m_historyId >= kMaxSpectrumHistory) {
                    m_historyId = 0;
                }

                if (m_historyId == 0 && (m_receivingData == false || m_receivingData)) {
                    std::fill(m_sampleAmplitudeAverage.begin(), m_sampleAmplitudeAverage.end(), 0.0f);
                    for (auto & s : m_sampleAmplitudeHistory) {
                        for (int i = 0; i < m_samplesPerFrame; ++i) {
                            m_sampleAmplitudeAverage[i] += s[i];
                        }
                    }
                    float norm = 1.0f/kMaxSpectrumHistory;
                    for (int i = 0; i < m_samplesPerFrame; ++i) {
                        m_sampleAmplitudeAverage[i] *= norm;
                    }

                    // calculate spectrum
                    std::copy(m_sampleAmplitudeAverage.begin(), m_sampleAmplitudeAverage.begin() + m_samplesPerFrame, m_fftIn.data());

                    FFT(m_fftIn.data(), m_fftOut.data(), m_samplesPerFrame, 1.0);

                    double fsum = 0.0;
                    for (int i = 0; i < m_samplesPerFrame; ++i) {
                        m_sampleSpectrum[i] = (m_fftOut[i].real()*m_fftOut[i].real() + m_fftOut[i].imag()*m_fftOut[i].imag());
                        fsum += m_sampleSpectrum[i];
                    }
                    for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                        m_sampleSpectrum[i] += m_sampleSpectrum[m_samplesPerFrame - i];
                    }
                }

                if (m_framesLeftToRecord > 0) {
                    std::copy(m_sampleAmplitude.begin(),
                              m_sampleAmplitude.begin() + m_samplesPerFrame,
                              m_recordedAmplitude.data() + (m_framesToRecord - m_framesLeftToRecord)*m_samplesPerFrame);

                    if (--m_framesLeftToRecord <= 0) {
                        m_analyzingData = true;
                    }
                }
            }

            if (m_analyzingData) {
                printf("Analyzing captured data ..\n");
                auto tStart = std::chrono::high_resolution_clock::now();

                const int stepsPerFrame = 16;
                const int step = m_samplesPerFrame/stepsPerFrame;

                std::unique_ptr<RS::ReedSolomon> rsData;

                bool isValid = false;
                for (const auto & rxProtocol : kTxProtocols) {
                    // skip Rx protocol if start frequency is different from detected one
                    if (rxProtocol.freqStart != m_markerFreqStart) {
                        continue;
                    }

                    std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0.0f);

                    m_framesToAnalyze = m_nMarkerFrames*stepsPerFrame;
                    m_framesLeftToAnalyze = m_framesToAnalyze;
                    for (int ii = m_nMarkerFrames*stepsPerFrame - 1; ii >= m_nMarkerFrames*stepsPerFrame/2; --ii) {
                        bool knownLength = false;

                        const int offsetStart = ii;
                        for (int itx = 0; itx < 1024; ++itx) {
                            int offsetTx = offsetStart + itx*rxProtocol.framesPerTx*stepsPerFrame;
                            if (offsetTx >= m_recvDuration_frames*stepsPerFrame || (itx + 1)*rxProtocol.bytesPerTx >= (int) m_txDataEncoded.size()) {
                                break;
                            }

                            std::copy(
                                    m_recordedAmplitude.begin() + offsetTx*step,
                                    m_recordedAmplitude.begin() + offsetTx*step + m_samplesPerFrame, m_fftIn.data());

                            for (int k = 1; k < rxProtocol.framesPerTx - 1; ++k) {
                                for (int i = 0; i < m_samplesPerFrame; ++i) {
                                    m_fftIn[i] += m_recordedAmplitude[(offsetTx + k*stepsPerFrame)*step + i];
                                }
                            }

                            FFT(m_fftIn.data(), m_fftOut.data(), m_samplesPerFrame, 1.0);

                            for (int i = 0; i < m_samplesPerFrame; ++i) {
                                m_sampleSpectrum[i] = (m_fftOut[i].real()*m_fftOut[i].real() + m_fftOut[i].imag()*m_fftOut[i].imag());
                            }
                            for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                                m_sampleSpectrum[i] += m_sampleSpectrum[m_samplesPerFrame - i];
                            }

                            uint8_t curByte = 0;
                            for (int i = 0; i < 2*rxProtocol.bytesPerTx; ++i) {
                                double freq = m_hzPerSample*rxProtocol.freqStart;
                                int bin = std::round(freq*m_ihzPerSample) + i*16;

                                int kmax = 0;
                                double amax = 0.0;
                                for (int k = 0; k < 16; ++k) {
                                    if (m_sampleSpectrum[bin + k] > amax) {
                                        kmax = k;
                                        amax = m_sampleSpectrum[bin + k];
                                    }
                                }

                                if (i%2) {
                                    curByte += (kmax << 4);
                                    m_txDataEncoded[itx*rxProtocol.bytesPerTx + i/2] = curByte;
                                    curByte = 0;
                                } else {
                                    curByte = kmax;
                                }
                            }

                            if (itx*rxProtocol.bytesPerTx > m_encodedDataOffset && knownLength == false) {
                                if ((m_rsLength->Decode(m_txDataEncoded.data(), m_rxData.data()) == 0) && (m_rxData[0] > 0 && m_rxData[0] <= 140)) {
                                    knownLength = true;
                                } else {
                                    break;
                                }
                            }
                        }

                        if (knownLength) {
                            rsData.reset(new RS::ReedSolomon(m_rxData[0], ::getECCBytesForLength(m_rxData[0])));

                            int decodedLength = m_rxData[0];
                            if (rsData->Decode(m_txDataEncoded.data() + m_encodedDataOffset, m_rxData.data()) == 0) {
                                if (m_rxData[0] != 0) {
                                    std::string s((char *) m_rxData.data(), decodedLength);

                                    printf("Decoded length = %d\n", decodedLength);
                                    printf("Received sound data successfully: '%s'\n", s.c_str());

                                    isValid = true;
                                    m_hasNewRxData = true;
                                    m_lastRxDataLength = decodedLength;
                                }
                            }
                        }

                        if (isValid) {
                            break;
                        }
                        --m_framesLeftToAnalyze;
                    }

                    if (isValid) break;
                }

                m_framesToRecord = 0;

                if (isValid == false) {
                    printf("Failed to capture sound data. Please try again\n");
                    m_framesToRecord = -1;
                }

                m_receivingData = false;
                m_analyzingData = false;

                std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0.0f);

                m_framesToAnalyze = 0;
                m_framesLeftToAnalyze = 0;

                auto tEnd = std::chrono::high_resolution_clock::now();
                printf("Time to analyze: %g ms\n", getTime_ms(tStart, tEnd));
            }

            // check if receiving data
            if (m_receivingData == false) {
                bool isReceiving = false;

                for (const auto & rxProtocol : kTxProtocols) {
                    bool isReceivingCur = true;

                    for (int i = 0; i < m_nBitsInMarker; ++i) {
                        double freq = bitFreq(rxProtocol, i);
                        int bin = std::round(freq*m_ihzPerSample);

                        if (i%2 == 0) {
                            if (m_sampleSpectrum[bin] <= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) isReceivingCur = false;
                        } else {
                            if (m_sampleSpectrum[bin] >= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) isReceivingCur = false;
                        }
                    }

                    if (isReceivingCur) {
                        m_markerFreqStart = rxProtocol.freqStart;
                        isReceiving = true;
                        break;
                    }
                }

                if (isReceiving) {
                    std::time_t timestamp = std::time(nullptr);
                    printf("%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));

                    m_rxData.fill(0);
                    m_receivingData = true;

                    // max recieve duration
                    m_recvDuration_frames =
                        2*m_nMarkerFrames + m_nPostMarkerFrames +
                        maxFramesPerTx()*((kMaxLength + ::getECCBytesForLength(kMaxLength))/minBytesPerTx() + 1);

                    m_framesToRecord = m_recvDuration_frames;
                    m_framesLeftToRecord = m_recvDuration_frames;
                }
            } else {
                bool isEnded = false;

                for (const auto & rxProtocol : kTxProtocols) {
                    bool isEndedCur = true;

                    for (int i = 0; i < m_nBitsInMarker; ++i) {
                        double freq = bitFreq(rxProtocol, i);
                        int bin = std::round(freq*m_ihzPerSample);

                        if (i%2 == 0) {
                            if (m_sampleSpectrum[bin] >= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) isEndedCur = false;
                        } else {
                            if (m_sampleSpectrum[bin] <= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) isEndedCur = false;
                        }
                    }

                    if (isEndedCur) {
                        isEnded = true;
                        break;
                    }
                }

                if (isEnded && m_framesToRecord > 1) {
                    std::time_t timestamp = std::time(nullptr);
                    printf("%sReceived end marker. Frames left = %d\n", std::asctime(std::localtime(&timestamp)), m_framesLeftToRecord);
                    m_recvDuration_frames -= m_framesLeftToRecord - 1;
                    m_framesLeftToRecord = 1;
                }
            }
        } else {
            break;
        }
    }
}

int GGWave::takeRxData(TxRxData & dst) {
    if (m_lastRxDataLength == 0) return 0;

    auto res = m_lastRxDataLength;
    m_lastRxDataLength = 0;
    dst = m_rxData;

    return res;
}

int GGWave::maxFramesPerTx() const {
    int res = 0;
    for (const auto & protocol : kTxProtocols) {
        res = std::max(res, protocol.framesPerTx);
    }
    return res;
}

int GGWave::minBytesPerTx() const {
    int res = kTxProtocols.front().framesPerTx;
    for (const auto & protocol : kTxProtocols) {
        res = std::min(res, protocol.bytesPerTx);
    }
    return res;
}

