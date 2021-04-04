
#include "Lektor2.h"
#include <driver/adc.h>
#include <freertos/ringbuf.h>
#include <WiFi.h>

// Zakomentuj linię poniżej jeśli nie używasz bluetooth
#define ENABLE_BT 1

// Tu możesz wstawić nazwę, pod którą będzie widziana suwmiarka
// w otoczeniu bluetooth
#define BT_NAME "ESP32Suwmiarka"

#ifdef ENABLE_BT
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
bool BTActive = false;
#endif

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
void stopAudio(void)
{
    audio.stop();
    while(audioRunning) delay(10);
}

#ifdef ENABLE_BT
char btControl = 'a';
uint8_t btSpeaking;
uint32_t btSent;
uint8_t btClient=0;
void nextBT(void)
{
    if (btControl < 'a') {
        btControl='a';
    }
    else if (btControl >= 'z') {
        btControl='a';
    }
    else {
        btControl++;
    }
    //printf("Control char = %c\n", btControl);
}
void stopBTSpeaking(void)
{
    if (BTActive && SerialBT.hasClient() && btSpeaking) {
        nextBT();
        SerialBT.write(btControl);
        SerialBT.println("");
        
    }
}
#endif


void say(const char *text, bool local=false)
{
    stopAudio();
#ifdef ENABLE_BT
    if (!local && BTActive && SerialBT.hasClient()) {
        nextBT();
        SerialBT.write(btControl);
        SerialBT.println(text);
        speaking = 1;
        btSpeaking = 1;
        btSent = millis();
        return;
    }
#endif
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
    lastKey = digitalRead(BUTTON_PIN);
#ifdef ENABLE_BT
    
    if (!lastKey) {
        SerialBT.begin(BT_NAME);
        while(!(lastKey = digitalRead(BUTTON_PIN))) delay(10);
        say("Połączenie blutuf aktywne", true);
        BTActive = true;
    }
    else
#endif    
    
    say("Gotowy do pracy");
    speaking=1;
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
        stopAudio();
#if BT_ENABLE
        stopBTSpeaking();
#endif
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
#ifdef ENABLE_BT
    if (btSpeaking && BTActive) {
        char btrcv;
        while (SerialBT.available()) {
            btrcv = SerialBT.read();
            //printf("BT received %c\n", btrcv);
            if (btrcv == btControl) {
                speaking = 0;
                btSpeaking=0;
            }
        }
        if (btSpeaking && millis() - btSent > 3000UL) {
            btSpeaking = 0;
        }
        if (!btSpeaking) {
            //printf("BT Speaking finished\n");
            lastSpoken = millis();
        }
    }
    else {
#endif
     if (audioRunning) {
        speaking=1;
        delay(10);
        return;
    }
        
    else if (speaking) {
        speaking=0;
        lastSpoken = millis();
    }
#ifdef ENABLE_BT
    }
#endif 
    if (!digitalRead(BUTTON_PIN)) return;
#ifdef ENABLE_BT
    if (!btClient && BTActive && SerialBT.hasClient()) {
        btClient = 1;
        say("Połączono z suwmiarką");
        return;
    }
#endif    
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
                else if (!speaking && vchanged && millis() - lastSpoken > 1000UL) {
                    vchanged = 0;
                    speak = 1;
                }
            }
        }
        else if (readMode == rmd_OnDemand) {
            if (key == key_Short) speak = 1;
        }
        else if (!speaking && millis() - lastSpoken > 1000UL) speak = 1;
        
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
