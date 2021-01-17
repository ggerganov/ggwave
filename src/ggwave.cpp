#include "ggwave/ggwave.h"

#include "reed-solomon/rs.hpp"

#include <chrono>
#include <algorithm>
#include <random>
#include <stdexcept>

void testC() {
    printf("Hello from C\n");
}

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

void ordina(float * f1, int N) {
    float f2[2*GGWave::kMaxSamplesPerFrame];
    for (int i = 0; i < N; i++) {
        int ir = reverse(N, i);
        f2[2*i + 0] = f1[2*ir + 0];
        f2[2*i + 1] = f1[2*ir + 1];
    }
    for (int j = 0; j < N; j++) {
        f1[2*j + 0] = f2[2*j + 0];
        f1[2*j + 1] = f2[2*j + 1];
    }
}

void transform(float * f, int N) {
    ordina(f, N);    //first: reverse order
    float * W;
    W = (float *)malloc(N*sizeof(float));
    W[2*1 + 0] = cos(-2.*M_PI/N);
    W[2*1 + 1] = sin(-2.*M_PI/N);
    W[2*0 + 0] = 1;
    W[2*0 + 1] = 0;
    for (int i = 2; i < N / 2; i++) {
        W[2*i + 0] = cos(-2.*i*M_PI/N);
        W[2*i + 1] = sin(-2.*i*M_PI/N);
    }
    int n = 1;
    int a = N / 2;
    for(int j = 0; j < log2(N); j++) {
        for(int i = 0; i < N; i++) {
            if(!(i & n)) {
                int wi = (i * a) % (n * a);
                int fi = i + n;
                float a = W[2*wi + 0];
                float b = W[2*wi + 1];
                float c = f[2*fi + 0];
                float d = f[2*fi + 1];
                float temp[2] = { f[2*i + 0], f[2*i + 1] };
                float Temp[2] = { a*c - b*d, b*c + a*d };
                f[2*i + 0]  = temp[0] + Temp[0];
                f[2*i + 1]  = temp[1] + Temp[1];
                f[2*fi + 0] = temp[0] - Temp[0];
                f[2*fi + 1] = temp[1] - Temp[1];
            }
        }
        n *= 2;
        a = a / 2;
    }
    free(W);
}

void FFT(float * f, int N, float d) {
    transform(f, N);
    for (int i = 0; i < N; i++) {
        f[2*i + 0] *= d;
        f[2*i + 1] *= d;
    }
}

void FFT(float * src, float * dst, int N, float d) {
    for (int i = 0; i < N; ++i) {
        dst[2*i + 0] = src[i];
        dst[2*i + 1] = 0.0f;
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
    m_fftIn(kMaxSamplesPerFrame),
    m_fftOut(2*kMaxSamplesPerFrame),
    m_hasNewSpectrum(false),
    m_sampleSpectrum(kMaxSamplesPerFrame),
    m_sampleAmplitude(kMaxSamplesPerFrame),
    m_hasNewRxData(false),
    m_lastRxDataLength(0),
    m_rxData(kMaxDataSize),
    m_sampleAmplitudeAverage(kMaxSamplesPerFrame),
    m_sampleAmplitudeHistory(kMaxSpectrumHistory),
    m_recordedAmplitude(kMaxRecordedFrames*kMaxSamplesPerFrame),
    m_txData(kMaxDataSize),
    m_txDataEncoded(kMaxDataSize),
    m_outputBlock(kMaxSamplesPerFrame),
    m_outputBlock16(kMaxRecordedFrames*kMaxSamplesPerFrame)
{
    if (samplesPerFrame > kMaxSamplesPerFrame) {
        throw std::runtime_error("Invalid samples per frame");
    }

    init(0, "", getDefultTxProtocol(), 0);
}

GGWave::~GGWave() {
}

bool GGWave::init(int textLength, const char * stext, const TxProtocol & aProtocol, const int volume) {
    if (textLength > kMaxLength) {
        fprintf(stderr, "Truncating data from %d to 140 bytes\n", textLength);
        textLength = kMaxLength;
    }

    m_txProtocol = aProtocol;
    m_txDataLength = textLength;
    m_sendVolume = ((double)(volume))/100.0f;

    const uint8_t * text = reinterpret_cast<const uint8_t *>(stext);

    m_hasNewTxData = false;
    std::fill(m_txData.begin(), m_txData.end(), 0);
    std::fill(m_txDataEncoded.begin(), m_txDataEncoded.end(), 0);

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

    std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0);
    std::fill(m_sampleAmplitude.begin(), m_sampleAmplitude.end(), 0);
    for (auto & s : m_sampleAmplitudeHistory) {
        s.resize(kMaxSamplesPerFrame);
        std::fill(s.begin(), s.end(), 0);
    }

    std::fill(m_rxData.begin(), m_rxData.end(), 0);

    for (int i = 0; i < m_samplesPerFrame; ++i) {
        m_fftOut[2*i + 0] = 0.0f;
        m_fftOut[2*i + 1] = 0.0f;
    }

    return true;
}

bool GGWave::send(const CBQueueAudio & cbQueueAudio) {
    int samplesPerFrameOut = (m_sampleRateOut/m_sampleRateIn)*m_samplesPerFrame;
    if (m_sampleRateOut > m_sampleRateIn) {
        fprintf(stderr, "Error: capture sample rate (%d Hz) must be <= playback sample rate (%d Hz)\n", (int) m_sampleRateIn, (int) m_sampleRateOut);
        return false;
    }
    if (m_sampleRateOut != m_sampleRateIn) {
        fprintf(stderr, "Resampling from %d Hz to %d Hz\n", (int) m_sampleRateIn, (int) m_sampleRateOut);
    }

    int frameId = 0;

    std::vector<double> phaseOffsets(kMaxDataBits);

    for (int k = 0; k < (int) phaseOffsets.size(); ++k) {
        phaseOffsets[k] = (M_PI*k)/(m_txProtocol.nDataBitsPerTx());
    }

    // note : what is the purpose of this shuffle ? I forgot .. :(
    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(phaseOffsets.begin(), phaseOffsets.end(), g);

    std::vector<bool> dataBits(kMaxDataBits);

    std::vector<AmplitudeData> bit1Amplitude(kMaxDataBits);
    std::vector<AmplitudeData> bit0Amplitude(kMaxDataBits);

    for (int k = 0; k < (int) dataBits.size(); ++k) {
        double freq = bitFreq(m_txProtocol, k);

        bit1Amplitude[k].resize(kMaxSamplesPerFrame);
        bit0Amplitude[k].resize(kMaxSamplesPerFrame);

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
    RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1);

    rsLength.Encode(m_txData.data(), m_txDataEncoded.data());
    rsData.Encode(m_txData.data() + 1, m_txDataEncoded.data() + m_encodedDataOffset);

    while (m_hasNewTxData) {
        std::fill(m_outputBlock.begin(), m_outputBlock.end(), 0.0f);

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
                    ::addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId, m_nMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId, m_nMarkerFrames);
                }
            }
        } else if (frameId < m_nMarkerFrames + m_nPostMarkerFrames) {
            nFreq = m_nBitsInMarker;

            for (int i = 0; i < m_nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId - m_nMarkerFrames, m_nPostMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, frameId - m_nMarkerFrames, m_nPostMarkerFrames);
                }
            }
        } else if (frameId <
                   (m_nMarkerFrames + m_nPostMarkerFrames) +
                   ((m_sendDataLength + m_nECCBytesPerTx)/m_txProtocol.bytesPerTx + 2)*m_txProtocol.framesPerTx) {
            int dataOffset = frameId - m_nMarkerFrames - m_nPostMarkerFrames;
            int cycleModMain = dataOffset%m_txProtocol.framesPerTx;
            dataOffset /= m_txProtocol.framesPerTx;
            dataOffset *= m_txProtocol.bytesPerTx;

            std::fill(dataBits.begin(), dataBits.end(), 0);

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
                    ::addAmplitudeSmooth(bit0Amplitude[k/2], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, cycleModMain, m_txProtocol.framesPerTx);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[k/2], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, cycleModMain, m_txProtocol.framesPerTx);
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
                    addAmplitudeSmooth(bit0Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, fId, m_nMarkerFrames);
                } else {
                    addAmplitudeSmooth(bit1Amplitude[i], m_outputBlock, m_sendVolume, 0, samplesPerFrameOut, fId, m_nMarkerFrames);
                }
            }
        } else {
            m_hasNewTxData = false;
        }

        if (nFreq == 0) nFreq = 1;
        float scale = 1.0f/nFreq;
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_outputBlock[i] *= scale;
        }

        // todo : support for non-int16 output
        for (int i = 0; i < samplesPerFrameOut; ++i) {
            m_outputBlock16[frameId*samplesPerFrameOut + i] = std::round(32000.0*m_outputBlock[i]);
        }

        ++frameId;
    }

    cbQueueAudio(m_outputBlock16.data(), frameId*samplesPerFrameOut*m_sampleSizeBytesOut);

    m_txAmplitudeData16.resize(frameId*samplesPerFrameOut);
    for (int i = 0; i < frameId*samplesPerFrameOut; ++i) {
        m_txAmplitudeData16[i] = m_outputBlock16[i];
    }

    return true;
}

void GGWave::receive(const CBDequeueAudio & CBDequeueAudio) {
    while (m_hasNewTxData == false) {
        // read capture data
        //
        // todo : support for non-float input
        auto nBytesRecorded = CBDequeueAudio(m_sampleAmplitude.data(), m_samplesPerFrame*m_sampleSizeBytesIn);

        if (nBytesRecorded != 0) {
            m_sampleAmplitudeHistory[m_historyId] = m_sampleAmplitude;

            if (++m_historyId >= kMaxSpectrumHistory) {
                m_historyId = 0;
            }

            if (m_historyId == 0 || m_receivingData) {
                m_hasNewSpectrum = true;

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

                for (int i = 0; i < m_samplesPerFrame; ++i) {
                    m_sampleSpectrum[i] = (m_fftOut[2*i + 0]*m_fftOut[2*i + 0] + m_fftOut[2*i + 1]*m_fftOut[2*i + 1]);
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

            if (m_analyzingData) {
                fprintf(stderr, "Analyzing captured data ..\n");
                auto tStart = std::chrono::high_resolution_clock::now();

                const int stepsPerFrame = 16;
                const int step = m_samplesPerFrame/stepsPerFrame;

                bool isValid = false;
                for (int rxProtocolId = 0; rxProtocolId < (int) getTxProtocols().size(); ++rxProtocolId) {
                    const auto & rxProtocol = getTxProtocols()[rxProtocolId];

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
                                m_sampleSpectrum[i] = (m_fftOut[2*i + 0]*m_fftOut[2*i + 0] + m_fftOut[2*i + 1]*m_fftOut[2*i + 1]);
                            }
                            for (int i = 1; i < m_samplesPerFrame/2; ++i) {
                                m_sampleSpectrum[i] += m_sampleSpectrum[m_samplesPerFrame - i];
                            }

                            uint8_t curByte = 0;
                            for (int i = 0; i < 2*rxProtocol.bytesPerTx; ++i) {
                                double freq = m_hzPerSample*rxProtocol.freqStart;
                                int bin = std::round(freq*m_ihzPerSample) + 16*i;

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
                                RS::ReedSolomon rsLength(1, m_encodedDataOffset - 1);
                                if ((rsLength.Decode(m_txDataEncoded.data(), m_rxData.data()) == 0) && (m_rxData[0] > 0 && m_rxData[0] <= 140)) {
                                    knownLength = true;
                                } else {
                                    break;
                                }
                            }

                            if (knownLength && itx*rxProtocol.bytesPerTx > m_encodedDataOffset + m_rxData[0] + ::getECCBytesForLength(m_rxData[0]) + 1) {
                                break;
                            }
                        }

                        if (knownLength) {
                            int decodedLength = m_rxData[0];

                            RS::ReedSolomon rsData(decodedLength, ::getECCBytesForLength(decodedLength));

                            if (rsData.Decode(m_txDataEncoded.data() + m_encodedDataOffset, m_rxData.data()) == 0) {
                                if (m_rxData[0] != 0) {
                                    std::string s((char *) m_rxData.data(), decodedLength);

                                    fprintf(stderr, "Decoded length = %d\n", decodedLength);
                                    fprintf(stderr, "Received sound data successfully: '%s'\n", s.c_str());

                                    isValid = true;
                                    m_hasNewRxData = true;
                                    m_lastRxDataLength = decodedLength;
                                    m_rxProtocol = rxProtocol;
                                    m_rxProtocolId = rxProtocolId;
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
                    fprintf(stderr, "Failed to capture sound data. Please try again\n");
                    m_lastRxDataLength = -1;
                    m_framesToRecord = -1;
                }

                m_receivingData = false;
                m_analyzingData = false;

                std::fill(m_sampleSpectrum.begin(), m_sampleSpectrum.end(), 0.0f);

                m_framesToAnalyze = 0;
                m_framesLeftToAnalyze = 0;

                auto tEnd = std::chrono::high_resolution_clock::now();
                fprintf(stderr, "Time to analyze: %g ms\n", getTime_ms(tStart, tEnd));
            }

            // check if receiving data
            if (m_receivingData == false) {
                bool isReceiving = false;

                for (const auto & rxProtocol : getTxProtocols()) {
                    int nDetectedMarkerBits = m_nBitsInMarker;

                    for (int i = 0; i < m_nBitsInMarker; ++i) {
                        double freq = bitFreq(rxProtocol, i);
                        int bin = std::round(freq*m_ihzPerSample);

                        if (i%2 == 0) {
                            if (m_sampleSpectrum[bin] <= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                        } else {
                            if (m_sampleSpectrum[bin] >= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) --nDetectedMarkerBits;
                        }
                    }

                    if (nDetectedMarkerBits == m_nBitsInMarker) {
                        m_markerFreqStart = rxProtocol.freqStart;
                        isReceiving = true;
                        break;
                    }
                }

                if (isReceiving) {
                    std::time_t timestamp = std::time(nullptr);
                    fprintf(stderr, "%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));

                    m_receivingData = true;
                    std::fill(m_rxData.begin(), m_rxData.end(), 0);

                    // max recieve duration
                    m_recvDuration_frames =
                        2*m_nMarkerFrames + m_nPostMarkerFrames +
                        maxFramesPerTx()*((kMaxLength + ::getECCBytesForLength(kMaxLength))/minBytesPerTx() + 1);

                    m_framesToRecord = m_recvDuration_frames;
                    m_framesLeftToRecord = m_recvDuration_frames;
                }
            } else {
                bool isEnded = false;

                for (const auto & rxProtocol : getTxProtocols()) {
                    int nDetectedMarkerBits = m_nBitsInMarker;

                    for (int i = 0; i < m_nBitsInMarker; ++i) {
                        double freq = bitFreq(rxProtocol, i);
                        int bin = std::round(freq*m_ihzPerSample);

                        if (i%2 == 0) {
                            if (m_sampleSpectrum[bin] >= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                        } else {
                            if (m_sampleSpectrum[bin] <= 3.0f*m_sampleSpectrum[bin + m_freqDelta_bin]) nDetectedMarkerBits--;
                        }
                    }

                    if (nDetectedMarkerBits == m_nBitsInMarker) {
                        isEnded = true;
                        break;
                    }
                }

                if (isEnded && m_framesToRecord > 1) {
                    std::time_t timestamp = std::time(nullptr);
                    fprintf(stderr, "%sReceived end marker. Frames left = %d\n", std::asctime(std::localtime(&timestamp)), m_framesLeftToRecord);
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

    if (res != -1) {
        dst = m_rxData;
    }

    return res;
}

int GGWave::takeTxAmplitudeData16(AmplitudeData16 & dst) {
    if (m_txAmplitudeData16.size() == 0) return 0;

    int res = (int) m_txAmplitudeData16.size();
    dst = std::move(m_txAmplitudeData16);

    return res;
}

bool GGWave::takeSpectrum(SpectrumData & dst) {
    if (m_hasNewSpectrum == false) return false;

    m_hasNewSpectrum = false;
    dst = m_sampleSpectrum;

    return true;
}

int GGWave::maxFramesPerTx() const {
    int res = 0;
    for (const auto & protocol : getTxProtocols()) {
        res = std::max(res, protocol.framesPerTx);
    }
    return res;
}

int GGWave::minBytesPerTx() const {
    int res = getTxProtocols().front().framesPerTx;
    for (const auto & protocol : getTxProtocols()) {
        res = std::min(res, protocol.bytesPerTx);
    }
    return res;
}

