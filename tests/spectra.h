/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Khantipol Kasemsant; built on Felucca, (C) 2026 Leo Kuroshita, Hügelton Instruments */
/* Shared measurement helpers of the Forge Five engine tests (tests/ladder_test.c, tests/sem_test.c): an FFT, the
 * averaged power spectrum, a band's level, the spectral peak, the fundamental by zero crossings, RMS. NFFT first. */
/* ------------------------------------------------------------ spectra --- */
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                double xi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j + len / 2] = re[i + j] - xr, im[i + j + len / 2] = im[i + j] - xi;
                re[i + j] += xr, im[i + j] += xi;
            }
    }
}
/* the averaged power spectrum (Hann) of y (nf frames of NFFT) into ps[NFFT / 2] */
static void spectrum(const double *y, uint32_t nf, double *ps)
{
    static double re[NFFT], im[NFFT];
    uint32_t f, i;
    memset(ps, 0, sizeof(double) * NFFT / 2);
    for (f = 0; f < nf; f++) {
        for (i = 0; i < NFFT; i++) {
            re[i] = y[f * NFFT + i] * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
            im[i] = 0;
        }
        fft(re, im, NFFT);
        for (i = 0; i < NFFT / 2; i++)
            ps[i] += re[i] * re[i] + im[i] * im[i];
    }
}
static double band_db(const double *ps, double f0, double f1)    /* mean power per bin, f0..f1 Hz */
{
    uint32_t a = (uint32_t)(f0 * NFFT / FS), b = (uint32_t)(f1 * NFFT / FS), i;
    double s = 0;
    for (i = a; i <= b; i++)
        s += ps[i];
    return 10 * log10(s / (b - a + 1) + 1e-30);
}
/* the strongest spectral peak above f_min, interpolated (Hz) */
static double peak_hz(const double *y, double f_min)
{
    static double ps[NFFT / 2];
    uint32_t i, best = (uint32_t)(f_min * NFFT / FS) + 1u;
    double a, b, c, d;
    spectrum(y, 2, ps);
    for (i = best; i < NFFT / 2 - 1; i++)
        if (ps[i] > ps[best])
            best = i;
    a = log(ps[best - 1] + 1e-30), b = log(ps[best] + 1e-30), c = log(ps[best + 1] + 1e-30);
    d = (a - c) / (2 * (a - 2 * b + c));
    return (best + d) * FS / NFFT;
}
/* the fundamental by rising zero crossings of the DC-free signal (low tones) */
static double zc_hz(const double *y, uint32_t n)      /* (hysteresis: a quarter of the peak; the hiss is far below) */
{
    double m = 0, pk = 0, first = -1, last = -1, h;
    uint32_t i, c = 0;
    int armed = 0;
    for (i = 0; i < n; i++)
        m += y[i];
    m /= n;
    for (i = 0; i < n; i++)
        pk = fabs(y[i] - m) > pk ? fabs(y[i] - m) : pk;
    h = pk / 4;
    for (i = 1; i < n; i++) {
        if (y[i] - m < -h)
            armed = 1;
        if (armed && y[i - 1] - m < 0 && y[i] - m >= 0) {
            double t = i - 1 + (m - y[i - 1]) / (y[i] - y[i - 1]);
            if (first < 0)
                first = t;
            last = t;
            c++;
            armed = 0;
        }
    }
    return c > 1 ? (c - 1) * (double)FS / (last - first) : 0;
}
static double rms(const double *y, uint32_t n)
{
    double s = 0, m = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        m += y[i];
    m /= n;
    for (i = 0; i < n; i++)
        s += (y[i] - m) * (y[i] - m);
    return sqrt(s / n);
}
static double semis(double f, double ref) { return 12 * log2(f / ref); }

static double buf[8 * FS];

