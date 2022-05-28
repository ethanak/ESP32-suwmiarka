
#include "Lektor2.h"
#include "prefs.h"
#include "enow.h"
#include "kbd.h"
#include <driver/adc.h>
#include <freertos/ringbuf.h>
#include <WiFi.h>

// Zakomentuj linię poniżej jeśli nie używasz bluetooth
#define ENABLE_BT 1
#define ENABLE_ENOW 1

// Tu możesz wstawić nazwę, pod którą będzie widziana suwmiarka
// w otoczeniu bluetooth
//#define BT_NAME "ESP32Suwmiarka"

#ifdef ENABLE_BT
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
bool BTActive = false;
#endif

bool ENowActive = false;

#define AUDIO_PIN 19
// BUTTON_PIN in kbd.h

/*
 * Caliper decoding code borrowed from: https://github.com/MGX3D/EspDRO
 */
 
int dataPin = 36; // yellow
#define dataChannel ADC1_CHANNEL_0
int clockPin = 39; // green
#define clockChannel ADC1_CHANNEL_3
int voltPin = 35;
#define voltChannel ADC1_CHANNEL_7

// ADC threshold for 1.5V SPCsignals (at 6dB/11-bit, high comes to around 1570 in analogRead() )
// or 390 ad 6dB/9-bit raw read
//#define ADC_TRESHOLD 800
#define ADC_TRESHOLD 200

// timeout in milliseconds for a bit read ($TODO - change to micros() )
#define BIT_READ_TIMEOUT 100

// timeout for a packet read 
#define PACKET_READ_TIMEOUT 250

// Packet format: [0 .. 19]=data, 20=sign, [21..22]=unused?, 23=inch
#define PACKET_BITS       24

// capped read: -1 (timeout), 0, 1
uint32_t mkls,dif;
int getBit() {
    int data;

    int readTimeout = millis(); //+ BIT_READ_TIMEOUT;
    while (adc1_get_raw(clockChannel) > ADC_TRESHOLD) {
      if (millis() - readTimeout > BIT_READ_TIMEOUT)
        return -1;
    }
    while (adc1_get_raw(clockChannel) < ADC_TRESHOLD) {
      if (millis() - readTimeout > BIT_READ_TIMEOUT)
        return -1;
    }
    data = (adc1_get_raw(dataChannel) > ADC_TRESHOLD)?1:0;
    dif = micros() - mkls;
    mkls = micros();
    
    return data;
}

// read one full packet
long getPacket() 
{
    long packet = 0;
    int readTimeout = millis();// + PACKET_READ_TIMEOUT;

    int bitIndex = 0;
    while (bitIndex < PACKET_BITS) {
      int bit = getBit();
      if (bit < 0 ) {
        // bit read timeout: reset packet or bail out
        if (millis() - readTimeout > PACKET_READ_TIMEOUT) {
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
    if (!local) {
#ifdef ENABLE_ENOW
        if (ENowActive) {
            if (sayENow(text)) return;
        }
#endif
#ifdef ENABLE_BT
        if (BTActive && SerialBT.hasClient()) {
            if (pfs_extsynth == ESYN_BTA) {
                nextBT();
                SerialBT.write(btControl);
            }
            SerialBT.println(text);
            speaking = 1;
            btSpeaking = 1;
            btSent = millis();
            return;
        }
        if (sayBLE(text)) {
            speaking = 1;
            btSpeaking = 1;
            btSent = millis();
            return;
        }
#endif
    }
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
/*
enum {
    key_None = 0,
    key_Pressed,
    key_Short,
    key_Long,
    key_Double};
    
uint32_t keyTimer;
uint8_t currentKey, lastKey;
uint8_t dblmode;

enum {
    DME_NONE=0,
    DME_WAIT,
    DME_FORGET
};
*/

uint8_t readMode = rmd_Changes;


void setup(void)
{
    Serial.begin(115200);
    WiFi.mode(WIFI_MODE_NULL);
    //WiFi.mode(WIFI_STA);
    //WiFi.disconnect();
    initPrefs();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    adc1_config_width(ADC_WIDTH_BIT_11);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_6);
    adc1_config_channel_atten(ADC1_CHANNEL_3,ADC_ATTEN_DB_6);
    adc1_config_channel_atten(voltChannel,ADC_ATTEN_DB_11);
    
    SpeechRB = xRingbufferCreateNoSplit(256,4);
    xTaskCreatePinnedToCore(speechThread,"speech",10000,NULL,3,&speechHandle,0);
    delay(10);
    int lkey = initKeyboardSys();
    bool esyn = false;
    
    if (!lkey || extena == 1) {
        switch(pfs_extsynth) {
#ifdef ENABLE_BT
            case ESYN_BTA:
            case ESYN_BTT: 
                SerialBT.begin(pfs_btname);
                //while(!(lastKey = digitalRead(BUTTON_PIN))) delay(10);
                say("Połączenie blutuf aktywne", true);
                esyn = true;
                BTActive = true;
                break;

            case ESYN_BLT:
                initBLE();
                say("Połączenie b l e aktywne");
                esyn = true;
                break;

#endif

#ifdef ENABLE_ENOW
            case ESYN_METR:
                ENowActive = initENow();
                if (ENowActive) {
                    say("Połączenie z metrówką", true);
                    esyn=true;
                }
                break;
#endif
            case ESYN_NONE:
            break;
            default:
            break;
        }
    }
    if (!esyn) say("Gotowy do pracy");
//        printf("\r\nI AM READY %s\r\n", WiFi.macAddress().c_str());

    extena = 0;
    speaking=1;
    delay(2000);
    waitKeyUp();
    printf("\nGotowy\n");
}

uint32_t noSignalTime;

void anounceRM(void)
{
    static const char *amd[]={"Na żądanie","Odczyt ciągły","Odczyt zmian"};
    say(amd[readMode]);
}

int rawVolt = 0;

int batpercent()
{
    static const int16_t voltlevel[]={
    3250, 3300, 3350, 3400, 3450,
    3500, 3550, 3600, 3650, 3700,
    3703, 3706, 3710, 3713, 3716,
    3719, 3723, 3726, 3729, 3732,
    3735, 3739, 3742, 3745, 3748,
    3752, 3755, 3758, 3761, 3765,
    3768, 3771, 3774, 3777, 3781,
    3784, 3787, 3790, 3794, 3797,
    3800, 3805, 3811, 3816, 3821,
    3826, 3832, 3837, 3842, 3847,
    3853, 3858, 3863, 3868, 3874,
    3879, 3884, 3889, 3895, 3900,
    3906, 3911, 3917, 3922, 3928,
    3933, 3939, 3944, 3950, 3956,
    3961, 3967, 3972, 3978, 3983,
    3989, 3994, 4000, 4008, 4015,
    4023, 4031, 4038, 4046, 4054,
    4062, 4069, 4077, 4085, 4092,
    4100, 4111, 4122, 4133, 4144,
    4156, 4167, 4178, 4189, 4200};
    int mvolt = (int)(rawVolt * pfs_mvmpx),i;
    for (i=100; i>0;i--) {
        if (mvolt >= voltlevel[i-1])  break;
    }
    return i;

}

void announceBattery(bool noinput = false)
{
    char buf[32];
    sprintf(buf,"%sBateria %d %%", (noinput?"Brak odczytu, ":""),batpercent());
    say(buf);
}

long lastPacket = 0x7fffffffL;
uint32_t lastSpoken;
uint8_t lastInch=2;
uint8_t vchanged = 0;
uint8_t anoNoSignal = 0;
uint8_t noSignalCounter = 0;
void getBattery()
{
    static uint32_t batTimer = millis() - 2000UL;
    static int bvolt = 0;
    static int nvolt = 0;
    uint32_t ms = millis();
    
    if (!nvolt && ms - batTimer < 1000UL) return;
    bvolt += adc1_get_raw(voltChannel);
    nvolt += 1;
    if (nvolt >= 16) {
        batTimer = ms;
        rawVolt = (bvolt / nvolt);
        bvolt = 0;
        nvolt = 0;
        batTimer = ms;
    }
}


void loop()
{
    uint8_t inch,neg,key;
    serloop();
    loopBLE();
    getBattery();
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
        else if (key == key_Double) {
            announceBattery();
        }
        else if (speaking) {
            speaking=0;
            lastSpoken = millis();
        }
    }
#ifdef ENABLE_BT
    if (btSpeaking) {
        if (BTActive) {
            char btrcv;
            while (SerialBT.available()) {
                btrcv = SerialBT.read();
            //printf("BT received %c\n", btrcv);
                if (btrcv == btControl) {
                    speaking = 0;
                    btSpeaking=0;
                }
            }
        }
        if (btSpeaking && millis() - btSent > pfs_emillis) {
            speaking=0;
            btSpeaking = 0;
        }
        if (!btSpeaking) {
            //printf("BT Speaking finished\n");
            lastSpoken = millis();
        }
    }
    else
#endif
    if (!loopENow()) {
         if (audioRunning) {
            speaking=1;
            delay(10);
            return;
        }
            
        else if (speaking) {
            speaking=0;
            lastSpoken = millis();
        }
    }
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
        //printf("%08x %d\n", packet, dif);
        anoNoSignal = 0;
        noSignalCounter = 0;
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
        if (key == key_Short || !anoNoSignal ||
                (readMode != rmd_OnDemand && millis() - noSignalTime > ((noSignalCounter < 10) ? 10000UL:60000UL))) {
            noSignalTime = millis();
            anoNoSignal = 1;
            //say("Brak odczytu");
            if (noSignalCounter < 10) {
                say("Brak odczytu");
                noSignalCounter++;
            }
            else {
                announceBattery(true);
            }
            
        }
    }
}
