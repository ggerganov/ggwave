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
        int aSampleRateIn,
        int aSampleRateOut,
        int aSamplesPerFrame,
        int aSampleSizeBytesIn,
        int aSampleSizeBytesOut) {

    sampleRateIn = aSampleRateIn;
    sampleRateOut = aSampleRateOut;
    samplesPerFrame = aSamplesPerFrame;
    sampleSizeBytesIn = aSampleSizeBytesIn;
    sampleSizeBytesOut = aSampleSizeBytesOut;

    setTxMode(TxMode::VariableLength);
}

GGWave::~GGWave() {
    if (rsData) delete rsData;
    if (rsLength) delete rsLength;
}

bool GGWave::init(int textLength, const char * stext, const TxProtocol & aProtocol) {
    if (textLength > kMaxLength) {
        printf("Truncating data from %d to 140 bytes\n", textLength);
        textLength = kMaxLength;
    }

    txProtocol = aProtocol;

    const uint8_t * text = reinterpret_cast<const uint8_t *>(stext);
    frameId = 0;
    nIterations = 0;
    hasData = false;

    isamplesPerFrame = 1.0f/samplesPerFrame;
    sendVolume = ((double)(txProtocol.paramVolume))/100.0f;
    hzPerFrame = sampleRateIn/samplesPerFrame;
    ihzPerFrame = 1.0/hzPerFrame;

    nDataBitsPerTx = txProtocol.paramBytesPerTx*8;
    nECCBytesPerTx = (txMode == TxMode::FixedLength) ? txProtocol.paramECCBytesPerTx : getECCBytesForLength(textLength);

    framesToAnalyze = 0;
    framesLeftToAnalyze = 0;
    framesToRecord = 0;
    framesLeftToRecord = 0;
    nBitsInMarker = 16;
    nMarkerFrames = 16;
    nPostMarkerFrames = 0;
    sendDataLength = (txMode == TxMode::FixedLength) ? kDefaultFixedLength : textLength + 3;

    freqDelta_bin = txProtocol.paramFreqDelta/2;
    freqDelta_hz = hzPerFrame*txProtocol.paramFreqDelta;
    freqStart_hz = hzPerFrame*txProtocol.paramFreqStart;
    if (txProtocol.paramFreqDelta == 1) {
        freqDelta_bin = 1;
        freqDelta_hz *= 2;
    }

    outputBlock.fill(0);

    txData.fill(0);
    txDataEncoded.fill(0);

    for (int k = 0; k < (int) phaseOffsets.size(); ++k) {
        phaseOffsets[k] = (M_PI*k)/(nDataBitsPerTx);
    }

    // note : what is the purpose of this shuffle ? I forgot .. :(
    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(phaseOffsets.begin(), phaseOffsets.end(), g);

    for (int k = 0; k < (int) dataBits.size(); ++k) {
        double freq = freqStart_hz + freqDelta_hz*k;
        dataFreqs_hz[k] = freq;

        double phaseOffset = phaseOffsets[k];
        double curHzPerFrame = sampleRateOut/samplesPerFrame;
        double curIHzPerFrame = 1.0/curHzPerFrame;
        for (int i = 0; i < samplesPerFrame; i++) {
            double curi = i;
            bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*(freq*curIHzPerFrame) + phaseOffset);
        }
        for (int i = 0; i < samplesPerFrame; i++) {
            double curi = i;
            bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*((freq + hzPerFrame*freqDelta_bin)*curIHzPerFrame) + phaseOffset);
        }
    }

    if (rsData) delete rsData;
    if (rsLength) delete rsLength;

    if (txMode == TxMode::FixedLength) {
        rsData = new RS::ReedSolomon(kDefaultFixedLength, nECCBytesPerTx);
        rsLength = nullptr;
    } else {
        rsData = new RS::ReedSolomon(textLength, nECCBytesPerTx);
        rsLength = new RS::ReedSolomon(1, 2);
    }

    if (textLength > 0) {
        if (txMode == TxMode::FixedLength) {
            for (int i = 0; i < textLength; ++i) txData[i] = text[i];
            rsData->Encode(txData.data(), txDataEncoded.data());
        } else {
            txData[0] = textLength;
            for (int i = 0; i < textLength; ++i) txData[i + 1] = text[i];
            rsData->Encode(txData.data() + 1, txDataEncoded.data() + 3);
            rsLength->Encode(txData.data(), txDataEncoded.data());
        }

        hasData = true;
    }

    // Rx
    receivingData = false;
    analyzingData = false;

    sampleAmplitude.fill(0);
    sampleSpectrum.fill(0);
    for (auto & s : sampleAmplitudeHistory) {
        s.fill(0);
    }

    rxData.fill(0);

    for (int i = 0; i < samplesPerFrame; ++i) {
        fftOut[i].real(0.0f);
        fftOut[i].imag(0.0f);
    }

    return true;
}

void GGWave::send(const CBQueueAudio & cbQueueAudio) {
    int samplesPerFrameOut = (sampleRateOut/sampleRateIn)*samplesPerFrame;
    if (sampleRateOut != sampleRateIn) {
        printf("Resampling from %d Hz to %d Hz\n", (int) sampleRateIn, (int) sampleRateOut);
    }

    while (hasData) {
        int nBytesPerTx = nDataBitsPerTx/8;
        std::fill(outputBlock.begin(), outputBlock.end(), 0.0f);
        std::uint16_t nFreq = 0;

        if (sampleRateOut != sampleRateIn) {
            for (int k = 0; k < nDataBitsPerTx; ++k) {
                double freq = freqStart_hz + freqDelta_hz*k;

                double phaseOffset = phaseOffsets[k];
                double curHzPerFrame = sampleRateOut/samplesPerFrame;
                double curIHzPerFrame = 1.0/curHzPerFrame;
                for (int i = 0; i < samplesPerFrameOut; i++) {
                    double curi = (i + frameId*samplesPerFrameOut);
                    bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*(freq*curIHzPerFrame) + phaseOffset);
                }
                for (int i = 0; i < samplesPerFrameOut; i++) {
                    double curi = (i + frameId*samplesPerFrameOut);
                    bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*((freq + hzPerFrame*freqDelta_bin)*curIHzPerFrame) + phaseOffset);
                }
            }
        }

        if (frameId < nMarkerFrames) {
            nFreq = nBitsInMarker;

            for (int i = 0; i < nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId, nMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId, nMarkerFrames);
                }
            }
        } else if (frameId < nMarkerFrames + nPostMarkerFrames) {
            nFreq = nBitsInMarker;

            for (int i = 0; i < nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId - nMarkerFrames, nPostMarkerFrames);
                } else {
                    ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId - nMarkerFrames, nPostMarkerFrames);
                }
            }
        } else if (frameId <
                   (nMarkerFrames + nPostMarkerFrames) +
                   ((sendDataLength + nECCBytesPerTx)/nBytesPerTx + 2)*txProtocol.paramFramesPerTx) {
            int dataOffset = frameId - nMarkerFrames - nPostMarkerFrames;
            int cycleModMain = dataOffset%txProtocol.paramFramesPerTx;
            dataOffset /= txProtocol.paramFramesPerTx;
            dataOffset *= nBytesPerTx;

            dataBits.fill(0);

            if (txProtocol.paramFreqDelta > 1) {
                for (int j = 0; j < nBytesPerTx; ++j) {
                    for (int i = 0; i < 8; ++i) {
                        dataBits[j*8 + i] = txDataEncoded[dataOffset + j] & (1 << i);
                    }
                }

                for (int k = 0; k < nDataBitsPerTx; ++k) {
                    ++nFreq;
                    if (dataBits[k] == false) {
                        ::addAmplitudeSmooth(bit0Amplitude[k], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, txProtocol.paramFramesPerTx);
                        continue;
                    }
                    ::addAmplitudeSmooth(bit1Amplitude[k], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, txProtocol.paramFramesPerTx);
                }
            } else {
                for (int j = 0; j < nBytesPerTx; ++j) {
                    {
                        uint8_t d = txDataEncoded[dataOffset + j] & 15;
                        dataBits[(2*j + 0)*16 + d] = 1;
                    }
                    {
                        uint8_t d = txDataEncoded[dataOffset + j] & 240;
                        dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                    }
                }

                for (int k = 0; k < 2*nBytesPerTx*16; ++k) {
                    if (dataBits[k] == 0) continue;

                    ++nFreq;
                    if (k%2) {
                        ::addAmplitudeSmooth(bit0Amplitude[k/2], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, txProtocol.paramFramesPerTx);
                    } else {
                        ::addAmplitudeSmooth(bit1Amplitude[k/2], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, txProtocol.paramFramesPerTx);
                    }
                }
            }
        } else if (txMode == TxMode::VariableLength && frameId <
                   (nMarkerFrames + nPostMarkerFrames) +
                   ((sendDataLength + nECCBytesPerTx)/nBytesPerTx + 2)*txProtocol.paramFramesPerTx +
                   (nMarkerFrames)) {
            nFreq = nBitsInMarker;

            int fId = frameId - ((nMarkerFrames + nPostMarkerFrames) + ((sendDataLength + nECCBytesPerTx)/nBytesPerTx + 2)*txProtocol.paramFramesPerTx);
            for (int i = 0; i < nBitsInMarker; ++i) {
                if (i%2 == 0) {
                    addAmplitudeSmooth(bit0Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, fId, nMarkerFrames);
                } else {
                    addAmplitudeSmooth(bit1Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, fId, nMarkerFrames);
                }
            }
        } else {
            textToSend = "";
            hasData = false;
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
    cbQueueAudio(outputBlock16.data(), frameId*samplesPerFrameOut*sampleSizeBytesOut);
}

void GGWave::receive(const CBDequeueAudio & CBDequeueAudio) {
    auto tCallStart = std::chrono::high_resolution_clock::now();

    while (hasData == false) {
        // read capture data
        //
        // todo : support for non-float input
        auto nBytesRecorded = CBDequeueAudio(sampleAmplitude.data(), samplesPerFrame*sampleSizeBytesIn);


        if (nBytesRecorded != 0) {
            {
                sampleAmplitudeHistory[historyId] = sampleAmplitude;

                if (++historyId >= kMaxSpectrumHistory) {
                    historyId = 0;
                }

                if (historyId == 0 && (receivingData == false || (receivingData && txMode == TxMode::VariableLength))) {
                    std::fill(sampleAmplitudeAverage.begin(), sampleAmplitudeAverage.end(), 0.0f);
                    for (auto & s : sampleAmplitudeHistory) {
                        for (int i = 0; i < samplesPerFrame; ++i) {
                            sampleAmplitudeAverage[i] += s[i];
                        }
                    }
                    float norm = 1.0f/kMaxSpectrumHistory;
                    for (int i = 0; i < samplesPerFrame; ++i) {
                        sampleAmplitudeAverage[i] *= norm;
                    }

                    // calculate spectrum
                    std::copy(sampleAmplitudeAverage.begin(), sampleAmplitudeAverage.begin() + samplesPerFrame, fftIn.data());

                    FFT(fftIn.data(), fftOut.data(), samplesPerFrame, 1.0);

                    double fsum = 0.0;
                    for (int i = 0; i < samplesPerFrame; ++i) {
                        sampleSpectrum[i] = (fftOut[i].real()*fftOut[i].real() + fftOut[i].imag()*fftOut[i].imag());
                        fsum += sampleSpectrum[i];
                    }
                    for (int i = 1; i < samplesPerFrame/2; ++i) {
                        sampleSpectrum[i] += sampleSpectrum[samplesPerFrame - i];
                    }

                    if (fsum < 1e-10) {
                        totalBytesCaptured = 0;
                    } else {
                        totalBytesCaptured += nBytesRecorded;
                    }
                }

                if (framesLeftToRecord > 0) {
                    std::copy(sampleAmplitude.begin(),
                              sampleAmplitude.begin() + samplesPerFrame,
                              recordedAmplitude.data() + (framesToRecord - framesLeftToRecord)*samplesPerFrame);

                    if (--framesLeftToRecord <= 0) {
                        analyzingData = true;
                    }
                }
            }

            if (analyzingData) {
                const int stepsPerFrame = 16;
                const int step = samplesPerFrame/stepsPerFrame;

                bool isValid = false;
                for (const auto & rxProtocol : txProtocols) {
                    std::fill(sampleSpectrum.begin(), sampleSpectrum.end(), 0.0f);

                    framesToAnalyze = nMarkerFrames*stepsPerFrame;
                    framesLeftToAnalyze = framesToAnalyze;
                    for (int ii = nMarkerFrames*stepsPerFrame - 1; ii >= nMarkerFrames*stepsPerFrame/2; --ii) {
                        bool knownLength = txMode == TxMode::FixedLength;

                        const int offsetStart = ii;
                        const int encodedOffset = (knownLength) ? 0 : 3;

                        for (int itx = 0; itx < 1024; ++itx) {
                            int offsetTx = offsetStart + itx*rxProtocol.paramFramesPerTx*stepsPerFrame;
                            if (offsetTx >= recvDuration_frames*stepsPerFrame || (itx + 1)*rxProtocol.paramBytesPerTx >= (int) txDataEncoded.size()) {
                                break;
                            }

                            std::copy(
                                    recordedAmplitude.begin() + offsetTx*step,
                                    recordedAmplitude.begin() + offsetTx*step + samplesPerFrame, fftIn.data());

                            for (int k = 1; k < rxProtocol.paramFramesPerTx - 1; ++k) {
                                for (int i = 0; i < samplesPerFrame; ++i) {
                                    fftIn[i] += recordedAmplitude[(offsetTx + k*stepsPerFrame)*step + i];
                                }
                            }

                            FFT(fftIn.data(), fftOut.data(), samplesPerFrame, 1.0);

                            for (int i = 0; i < samplesPerFrame; ++i) {
                                sampleSpectrum[i] = (fftOut[i].real()*fftOut[i].real() + fftOut[i].imag()*fftOut[i].imag());
                            }
                            for (int i = 1; i < samplesPerFrame/2; ++i) {
                                sampleSpectrum[i] += sampleSpectrum[samplesPerFrame - i];
                            }

                            uint8_t curByte = 0;
                            if (rxProtocol.paramFreqDelta > 1) {
                                for (int i = 0; i < nDataBitsPerTx; ++i) {
                                    int k = i%8;
                                    int bin = std::round(dataFreqs_hz[i]*ihzPerFrame);
                                    if (sampleSpectrum[bin] > 1*sampleSpectrum[bin + freqDelta_bin]) {
                                        curByte += 1 << k;
                                    } else if (sampleSpectrum[bin + freqDelta_bin] > 1*sampleSpectrum[bin]) {
                                    } else {
                                    }
                                    if (k == 7) {
                                        txDataEncoded[itx*rxProtocol.paramBytesPerTx + i/8] = curByte;
                                        curByte = 0;
                                    }
                                }
                            } else {
                                for (int i = 0; i < 2*rxProtocol.paramBytesPerTx; ++i) {
                                    int bin = std::round(dataFreqs_hz[0]*ihzPerFrame) + i*16;

                                    int kmax = 0;
                                    double amax = 0.0;
                                    for (int k = 0; k < 16; ++k) {
                                        if (sampleSpectrum[bin + k] > amax) {
                                            kmax = k;
                                            amax = sampleSpectrum[bin + k];
                                        }
                                    }

                                    if (i%2) {
                                        curByte += (kmax << 4);
                                        txDataEncoded[itx*rxProtocol.paramBytesPerTx + i/2] = curByte;
                                        curByte = 0;
                                    } else {
                                        curByte = kmax;
                                    }
                                }
                            }

                            if (txMode == TxMode::VariableLength) {
                                if (itx*rxProtocol.paramBytesPerTx > 3 && knownLength == false) {
                                    if ((rsLength->Decode(txDataEncoded.data(), rxData.data()) == 0) && (rxData[0] > 0 && rxData[0] <= 140)) {
                                        knownLength = true;
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }

                        if (txMode == TxMode::VariableLength && knownLength) {
                            if (rsData) delete rsData;
                            rsData = new RS::ReedSolomon(rxData[0], ::getECCBytesForLength(rxData[0]));
                        }

                        if (knownLength) {
                            int decodedLength = rxData[0];
                            if (rsData->Decode(txDataEncoded.data() + encodedOffset, rxData.data()) == 0) {
                                if (txMode == TxMode::FixedLength && rxData[0] == 'A') {
                                    printf("[ANSWER] Received sound data successfully!\n");
                                } else if (txMode == TxMode::FixedLength && rxData[0] == 'O') {
                                    printf("[OFFER]  Received sound data successfully!\n");
                                } else if (rxData[0] != 0) {
                                    printf("Decoded length = %d\n", decodedLength);
                                    std::string s((char *) rxData.data(), decodedLength);
                                    printf("Received sound data successfully: '%s'\n", s.c_str());

                                    isValid = true;
                                    hasNewRxData = true;
                                    lastRxDataLength = decodedLength;
                                }
                            }
                        }

                        if (isValid) {
                            break;
                        }
                        --framesLeftToAnalyze;
                    }

                    if (isValid) break;
                }

                framesToRecord = 0;

                if (isValid == false) {
                    printf("Failed to capture sound data. Please try again\n");
                    framesToRecord = -1;
                }

                receivingData = false;
                analyzingData = false;

                std::fill(sampleSpectrum.begin(), sampleSpectrum.end(), 0.0f);

                framesToAnalyze = 0;
                framesLeftToAnalyze = 0;
            }

            // check if receiving data
            if (receivingData == false) {
                bool isReceiving = true;

                for (int i = 0; i < nBitsInMarker; ++i) {
                    int bin = std::round(dataFreqs_hz[i]*ihzPerFrame);

                    if (i%2 == 0) {
                        if (sampleSpectrum[bin] <= 3.0f*sampleSpectrum[bin + freqDelta_bin]) isReceiving = false;
                    } else {
                        if (sampleSpectrum[bin] >= 3.0f*sampleSpectrum[bin + freqDelta_bin]) isReceiving = false;
                    }
                }

                if (isReceiving) {
                    std::time_t timestamp = std::time(nullptr);
                    printf("%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));
                    rxData.fill(0);
                    receivingData = true;
                    if (txMode == TxMode::FixedLength) {
                        recvDuration_frames = nMarkerFrames + nPostMarkerFrames + maxFramesPerTx()*((kDefaultFixedLength + maxECCBytesPerTx())/minBytesPerTx() + 1);
                    } else {
                        recvDuration_frames = nMarkerFrames + nPostMarkerFrames + maxFramesPerTx()*((kMaxLength + ::getECCBytesForLength(kMaxLength))/minBytesPerTx() + 1);
                    }
                    framesToRecord = recvDuration_frames;
                    framesLeftToRecord = recvDuration_frames;
                }
            } else if (txMode == TxMode::VariableLength) {
                bool isEnded = true;

                for (int i = 0; i < nBitsInMarker; ++i) {
                    int bin = std::round(dataFreqs_hz[i]*ihzPerFrame);

                    if (i%2 == 0) {
                        if (sampleSpectrum[bin] >= 3.0f*sampleSpectrum[bin + freqDelta_bin]) isEnded = false;
                    } else {
                        if (sampleSpectrum[bin] <= 3.0f*sampleSpectrum[bin + freqDelta_bin]) isEnded = false;
                    }
                }

                if (isEnded && framesToRecord > 1) {
                    std::time_t timestamp = std::time(nullptr);
                    printf("%sReceived end marker\n", std::asctime(std::localtime(&timestamp)));
                    recvDuration_frames -= framesLeftToRecord - 1;
                    framesLeftToRecord = 1;
                }
            }
        } else {
            break;
        }

        ++nIterations;
    }

    auto tCallEnd = std::chrono::high_resolution_clock::now();
    tSum_ms += getTime_ms(tCallStart, tCallEnd);
    if (++nCalls == 10) {
        averageRxTime_ms = tSum_ms/nCalls;
        tSum_ms = 0.0f;
        nCalls = 0;
    }
}
