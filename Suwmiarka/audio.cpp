#include <Arduino.h>
#include <driver/i2s.h>
#include "Lektor2.h"

#include "audio.h"

/* simplified code from ESP8266Audio library */

void AudioOut::DeltaSigma(int16_t sample, uint32_t dsBuff[8])
{
  fixed24p8_t newSamp = ( (int32_t)sample ) << 8;

  int oversample32 = oversample / 32;
  // How much the comparison signal changes each oversample step
  fixed24p8_t diffPerStep = (newSamp - lastSamp) >> (4 + oversample32);

  // Don't need lastSamp anymore, store this one for next round
  lastSamp = newSamp;

  for (int j = 0; j < oversample32; j++) {
    uint32_t bits = 0; // The bits we convert the sample into, MSB to go on the wire first
    
    for (int i = 32; i > 0; i--) {
      bits = bits << 1;
      if (cumErr < 0) {
        bits |= 1;
        cumErr += fixedPosValue - newSamp;
      } else {
        // Bits[0] = 0 handled already by left shift
        cumErr -= fixedPosValue + newSamp;
      }
      newSamp += diffPerStep; // Move the reference signal towards destination
    }
    dsBuff[j] = bits;
  }
}


int AudioOut::setSigmaDeltaPin(int pin)
{
    sigdelPin = pin;
    return 1;
}

int minv, maxv;
int AudioOut::init_audio(void)
{
    int i;
    size_t dummyout;
    uint32_t dsBuff[8];
    audioStop=false;
    for (i=-32600; i<=0; i+=20) {
        DeltaSigma(i, dsBuff);
        i2s_write((i2s_port_t)portNo,
                (void *)dsBuff,
                sizeof(uint32_t) * (oversample/32),
                &dummyout,
                portMAX_DELAY);
    }
    minv=0;
    maxv=0;
    return 0;
}

int AudioOut::put_sample(float sample)
{
    sample = sample * float_gain;
    int temp = (use_contrast?sin(sample + float_contrast*sin(4 * sample)):sample) * 32767;
    if (temp < -32767) {
        temp = -32767;
    }
    else if (temp > 32767) {
        temp = 32767;
    }
    return put_sample((int16_t)temp);
}

int AudioOut::put_sample(short int sample)
{
    uint32_t dsBuff[8];
    size_t dummyout;
    //static int16_t last_sample=0;
    if (minv > sample) minv=sample;
    if (maxv < sample) maxv=sample;
    for (int i = 0; i< 2; i++) {
       // DeltaSigma(last_sample+((sample-last_sample) * (i+1))/4, dsBuff);
         DeltaSigma(sample, dsBuff);
         i2s_write((i2s_port_t)portNo,
                (void *)dsBuff,
                sizeof(uint32_t) * (oversample/32),
                &dummyout,
                portMAX_DELAY);
    }
    if (audioStop) {
        return 1;
    }
    return 0;
}

void AudioOut::finalize_audio(void)
{
    int i;
    size_t dummyout;
    uint32_t dsBuff[8];
    for (i=0; i>-32600; i-=50) {
        DeltaSigma(i, dsBuff);
        i2s_write((i2s_port_t)portNo,
                (void *)dsBuff,
                sizeof(uint32_t) * (oversample/32),
                &dummyout,
                portMAX_DELAY);
    }
    delay(10);
    i2s_zero_dma_buffer((i2s_port_t)portNo);
}

void AudioOut::setbeep(int freq)
{
    _beepfreq=constrain(freq, 220, 880);
}
void AudioOut::beep(int freq)
{
    audioStop=false;
    freq = constrain(freq,220,880);
    float frs, frt = 11050.0 / 440;
    
    float phase=0, phase2=0;
    _beepfreq=freq;
    bool ctl = true;
    while (ctl) {
        int i;
        frs = 11050.0 / _beepfreq;
        for (i=0; i<128; phase = fmodf((phase+1), frs), phase2 = fmodf((phase2+1), frt), i++) {
            int16_t t = (phase > frs/2)?3276:-3276;
            if (_beepfreq == 440) {
                t = 1.1 * t;
            }
            else {
                t += (phase2 > frt/2)?3276:-3276;
            }
            if (put_sample(t)) {
                ctl = false;
                break;
            }
        }
        delay(1);
    }
    delay(10);
    i2s_zero_dma_buffer((i2s_port_t)portNo);
    
}


void AudioOut::SetPin(int dpin)
{
    i2s_pin_config_t pins = {
        .bck_io_num = -1,
        .ws_io_num = -1,
        .data_out_num = dpin,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin((i2s_port_t)portNo, &pins);
}

AudioOut::AudioOut(int pin, int port, int dma_buf_count, int use_apll)
{
    sigdelPin=pin;
    portNo = port;
    i2sOn = false;
    audioStop=false;
    dma_buf_count = dma_buf_count;
    oversample=32;
    float_size=32768.0;
    float_gain_in=4.2;
    float_gain=float_gain_in / float_size;
    float_contrast = 0.1;
    use_contrast = true;
    if (!i2sOn) {
        if (use_apll == APLL_AUTO) {
      // don't use audio pll on buggy rev0 chips
            use_apll = APLL_DISABLE;
            esp_chip_info_t out_info;
            esp_chip_info(&out_info);
            if(out_info.revision > 0) {
                use_apll = APLL_ENABLE;
            }
        }

        i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_comm_format_t comm_fmt = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);

        i2s_config_t i2s_config_dac = {
            .mode = mode,
            .sample_rate = 22050, //44100,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = comm_fmt,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
            .dma_buf_count = dma_buf_count,
            .dma_buf_len = 64,
            .use_apll = use_apll // Use audio PLL
        };
        printf("+%d %p\n", portNo, &i2s_config_dac);
        if (i2s_driver_install((i2s_port_t)portNo, &i2s_config_dac, 0, NULL) != ESP_OK) {
            printf("ERROR: Unable to install I2S drives\n");
        }
        SetPin(sigdelPin);
    }
    i2s_zero_dma_buffer((i2s_port_t)portNo);
}

AudioOut::~AudioOut()
{
    if (i2sOn) {
        printf("UNINSTALL I2S\n");
        i2s_driver_uninstall((i2s_port_t)portNo); //stop & destroy i2s driver
    }
    i2sOn = false;
}
