#include "Lektor2.h"
#include <driver/adc.h>
#include <freertos/ringbuf.h>
#include <WiFi.h>

#define AUDIO_PIN 14
#define BUTTON_PIN 32
/*
 * Caliper decoding code borrowed from: https://github.com/MGX3D/EspDRO
 */
 
int dataPin = 36; // yellow
int clockPin = 39; // green

// ADC threshold for 1.5V SPCsignals (at 6dB/11-bit, high comes to around 1570 in analogRead() )
#define ADC_TRESHOLD 800


// timeout in milliseconds for a bit read ($TODO - change to micros() )
#define BIT_READ_TIMEOUT 100

// timeout for a packet read 
#define PACKET_READ_TIMEOUT 250

// Packet format: [0 .. 19]=data, 20=sign, [21..22]=unused?, 23=inch
#define PACKET_BITS       24

// capped read: -1 (timeout), 0, 1
int getBit() {
    int data;

    int readTimeout = millis() + BIT_READ_TIMEOUT;
    while (analogRead(clockPin) > ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    while (analogRead(clockPin) < ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    data = (analogRead(dataPin) > ADC_TRESHOLD)?1:0;
    return data;
}

// read one full packet
long getPacket() 
{
    long packet = 0;
    int readTimeout = millis() + PACKET_READ_TIMEOUT;

    int bitIndex = 0;
    while (bitIndex < PACKET_BITS) {
      int bit = getBit();
      if (bit < 0 ) {
        // bit read timeout: reset packet or bail out
        if (millis() > readTimeout) {
          // packet timeout
          return -1;
        }
        
        bitIndex = 0;
        packet = 0;
        continue;
      }

      packet |= (bit & 1)<<bitIndex;
      
      bitIndex++;
    }
    
    return packet;
}

/*
 * End borrowed code
 */
 
long getResult(long packet, uint8_t *inch, uint8_t *neg)
{
  long data = packet & 0xFFFFF;
  *neg = (packet & 0x100000) ? 1 : 0;
  if (packet & 0x800000) {
        *inch = 1;
        data = data/2;
  }
  else *inch=0;
  return data;
}



Lektor2 spiker;
AudioOut audio(AUDIO_PIN);
RingbufHandle_t SpeechRB;
volatile bool audioRunning;
TaskHandle_t speechHandle;

struct spikerCmd {
    uint8_t cmd;
    char text[255];
    };
    
void speechThread(void *v)
{
    size_t itemsize;
    spiker.setAudioModule(&audio);
    struct spikerCmd *rbc;
    for (;;) {
        audioRunning = false;
        rbc = (struct spikerCmd *) xRingbufferReceive(SpeechRB, &itemsize, portMAX_DELAY);
        if (rbc) {
            audioRunning=true;
            if (rbc->cmd) spiker.say(rbc->text);
            else {
                int freq = (rbc->text[0] & 255) | ((rbc->text[1] << 8) & 0xff00);
                spiker.beep(freq);
            }
            vRingbufferReturnItem(SpeechRB, (void *)rbc);
        }
    }
}

uint8_t speaking;
void stopAudio(bool speech = false)
{
    audio.stop();
    while(audioRunning) delay(10);
}

void say(const char *text)
{
    stopAudio(true);
    struct spikerCmd rbc;
    rbc.cmd = 1;
    strcpy(rbc.text, text);
    xRingbufferSend(SpeechRB, &rbc, sizeof(rbc), 0);
    speaking=1;

}

enum {
    rmd_OnDemand = 0,
    rmd_Cont,
    rmd_Changes};

enum {
    key_None = 0,
    key_Pressed,
    key_Short,
    key_Long};
    
uint32_t keyTimer;
uint8_t currentKey, lastKey;
uint8_t readMode = rmd_Changes;
void setup(void)
{
    Serial.begin(115200);
    WiFi.mode(WIFI_MODE_NULL);
    
    pinMode(dataPin, INPUT);     
    pinMode(clockPin, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    analogReadResolution(11);
    
  
    analogSetAttenuation(ADC_6db);
    adc1_config_width(ADC_WIDTH_BIT_10);
    
    SpeechRB = xRingbufferCreateNoSplit(256,4);
    xTaskCreatePinnedToCore(speechThread,"speech",10000,NULL,3,&speechHandle,0);
    delay(10);
    say("Gotowy do pracy. odczyt zmian");
    speaking=1;
    lastKey = digitalRead(BUTTON_PIN);
    keyTimer = millis();
}

int getKey(void)
{
    uint32_t ms = millis();
    uint8_t rc;
    if (ms - keyTimer < 50) return key_None;
    currentKey = digitalRead(BUTTON_PIN);
    if (currentKey == lastKey) return key_None;
    lastKey = currentKey;
    if (!currentKey) rc = key_Pressed;
    else if (ms - keyTimer > 500UL) rc=key_Long;
    else rc=key_Short;
    keyTimer = ms;
    return rc;
}

uint32_t noSignalTime;

void anounceRM(void)
{
    static const char *amd[]={"Na żądanie","Odczyt ciągły","Odczyt zmian"};
    say(amd[readMode]);
}


long lastPacket = 0x7fffffffL;
uint32_t lastSpoken;
uint8_t lastInch=2;
uint8_t vchanged = 0;
uint8_t anoNoSignal = 0;

void loop()
{
    uint8_t inch,neg,key;
    key=getKey();
    if (key) {
        stopAudio(true);
        if (key == key_Long) {
            readMode = (readMode + 1) % 3;
            anounceRM();
            lastPacket = 0x7fffffffL;
            vchanged = 1;
        }
        else if (speaking) {
            speaking=0;
            lastSpoken = millis();
        }
    }
    if (audioRunning) {
        speaking=1;
        delay(10);
        return;
    }
    else if (speaking) {
        speaking=0;
        lastSpoken = millis();
    }
    
    long packet = getPacket();
    if (packet >= 0) {
        anoNoSignal = 0;
        noSignalTime = millis();
        uint8_t speak=0;
        if ((readMode == rmd_Changes)) {
            if (key == key_Short) {
                speak = 1;
                vchanged = 0;
            }
            else {
                if (packet != lastPacket) {
                    vchanged = 1;
                    lastPacket = packet;
                }
                else if (vchanged && millis() - lastSpoken > 1000UL) {
                    vchanged = 0;
                    speak = 1;
                }
            }
        }
        else if (readMode == rmd_OnDemand) {
            if (key == key_Short) speak = 1;
        }
        else if (millis() - lastSpoken > 1000UL) speak = 1;
        
        if (speak) {
            char buf[32],*c=buf;
            int n=getResult(packet,&inch,&neg);
            if (neg) {
                strcpy(buf,"minus ");
                c+=6;
            }
            if (inch) sprintf(c,"%d i %03d", n/1000, n%1000);
            else sprintf(c,"%d i %02d", n/100, n%100);
            if (lastInch != inch) {
                strcat(c, inch ? " cala" : " milimetra");
                lastInch = inch;
            }
            say(buf);
            lastPacket = packet;
        }
    }
    else {
        if (key == key_Short || !anoNoSignal || (readMode != rmd_OnDemand && millis() - noSignalTime > 10000UL)) {
            noSignalTime = millis();
            anoNoSignal = 1;
            say("Brak odczytu");
        }
    }
}
