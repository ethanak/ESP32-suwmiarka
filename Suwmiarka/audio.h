#ifndef LEK_AUDIO_H
#define LEK_AUDIO_H
#include <Arduino.h>

class AudioOut
{
    public:
    AudioOut(int pin, int port=0,  int dma_buf_count = 8, int use_apll=APLL_DISABLE);
    ~AudioOut();
    int setSigmaDeltaPin(int);
    int init_audio(void);
    int put_sample(int16_t sample);
    int put_sample(float sample);
    void finalize_audio(void);
    void stop() { audioStop=true; };
    void beep(int freq=440);
    void setbeep(int freq);
    enum : int { APLL_AUTO = -1, APLL_ENABLE = 1, APLL_DISABLE = 0 };
    void SetPin(int dpin);

    void setGain(float gain) {float_gain = (float_gain_in = gain) / float_size;};
    void setContrast(float contrast) {float_contrast = contrast; use_contrast = (contrast > 0.01);};
    void setContrast(int contrast) {float_contrast = contrast/100.0;use_contrast = (contrast != 0);};
    void setSampleSize(float size) {float_gain = float_gain_in /(float_size = size);};

    protected:
    int sigdelPin;
    uint8_t oversample;
    void DeltaSigma(int16_t sample, uint32_t dsBuff[4]);
    typedef int32_t fixed24p8_t;
    enum {fixedPosValue=0x007fff00}; /* 24.8 of max-signed-int */
    fixed24p8_t lastSamp; // Last sample value
    fixed24p8_t cumErr;   // Running cumulative error since time began};

    float float_gain;
    float float_gain_in;
    float float_contrast;
    float float_size;
    bool use_contrast;
    uint8_t portNo;
    bool mono;
    bool i2sOn;
    int dma_buf_count;
    volatile bool audioStop;
    volatile int _beepfreq;
};
#endif
