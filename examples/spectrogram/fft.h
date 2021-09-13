#pragma once

#include <cmath>
#include <stdlib.h>

// FFT routines taken from https://stackoverflow.com/a/37729648/4039976

constexpr auto kMaxSamplesPerFrame = 1024;

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
    float f2[2*kMaxSamplesPerFrame];
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
