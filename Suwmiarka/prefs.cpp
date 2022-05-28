#include <Arduino.h>
#include <Preferences.h>
#include "Lektor2.h"
#include "prefs.h"
#include <WiFi.h>
#include <errno.h>

char pfs_btname[16];
int pfs_tempo;
int pfs_extsynth;
int pfs_emillis;
uint8_t enmac[6];
bool pfs_have_enmac;
bool pfs_batt;
float pfs_mvmpx; 
char pfs_uidstr[3][40];
uint8_t pfs_uid[16];
extern int rawVolt;
extern int batpercent();

Preferences preferences;
RTC_NOINIT_ATTR int extena;
extern Lektor2 spiker;

static void genUUID()
{
    esp_fill_random(pfs_uid, 16);
    pfs_uid[6] = 0x40 | (pfs_uid[6] & 0xF);
    pfs_uid[8] = (0x80 | pfs_uid[8]) & ~0x40;
    pfs_uid[7] &= 0xfc;
}

static void mkuuids()
{
    int i;
    for (i=0;i<2;i++) {
        sprintf(pfs_uidstr[i],"%02x%02x%02x%02x-%02x%02x-%02x%02x-"
             "%02x%02x-%02x%02x%02x%02x%02x%02x",
             pfs_uid[0], pfs_uid[1], pfs_uid[2], pfs_uid[3],
             pfs_uid[4], pfs_uid[5], pfs_uid[6], pfs_uid[7]+i,
             pfs_uid[8], pfs_uid[9], pfs_uid[10], pfs_uid[11],
             pfs_uid[12], pfs_uid[13], pfs_uid[14], pfs_uid[15]);
    }
}

void initPrefs()
{
    preferences.begin("suwmiarka");
    if (!preferences.getString("bt",pfs_btname,15)) strcpy(pfs_btname,"ESPSuwmiarka");
    pfs_tempo = preferences.getInt("tempo",5);
    pfs_have_enmac=preferences.getBytes("enmacspkr",(void *)enmac,6);
    pfs_extsynth = preferences.getInt("extsynth",ESYN_BTT);
    pfs_emillis = preferences.getInt("extmillis",1500);
    //pfs_batt = preferences.getBool("batt",false);
    pfs_mvmpx = preferences.getFloat("volt",2.7);
    pfs_doubleMsec = preferences.getInt("dmsec", 300);
    pfs_longMsec = preferences.getInt("lmsec", 500);
    
    /*
    if (!preferences.getBytes("bleuuid",(void *)pfs_uid,16)) {
        genUUID();
        preferences.putBytes("bleuuid",(void *)pfs_uid,16);
    }
    mkuuids();
    */
}

void pfsSetTempo(int tempo)
{
    if (tempo < 0 || tempo > 15) return;
    pfs_tempo = tempo;
    preferences.putInt("tempo",tempo);
    spiker.setTempo(tempo);
}


static char buf[1024];
static int bufpos=0;
static char *tstr;

static int getser(void)
{

    while (Serial.available()) {
        int z=Serial.read();
        if (z == 13 || z == 10) {
            if (!bufpos) continue;
            buf[bufpos]= 0;
            bufpos=0;
            return 1;
        }
        if (bufpos < 1023) buf[bufpos++]=z;
    }
    return 0;
}

static char *getToken(bool required=true)
{
    char *cmd;
    while (*tstr && isspace(*tstr)) tstr++;
    if (!*tstr) {
        if (required) printf("Brak wymaganego parametru\n");
        return NULL;
    }
    cmd = tstr;
    while (*tstr && !isspace(*tstr)) tstr++;
    if (*tstr) {
        *tstr++=0;
        while (*tstr && isspace(*tstr)) tstr++;
    }
    return cmd;
}

static int getUInt(bool required=true, int minval=0, int maxval=9999)
{
    char *t=getToken(required);
    if (!t) return -1;
    char *t1;
    
    int ival = strtol(t,&t1,10);
    if (*t1) {
        printf("Błędny zapis liczby: %s\n", t);
        return -2;
    }
    if (ival < minval || ival > maxval) {
        printf("Liczba %d poza zakresem %d .. %d\n", ival, minval, maxval);
        return -2;
    }
    return ival;
}

extern void say(const char *text, bool local=false);
static void sersayme()
{
    if (!*tstr) {
        printf("Co ja miałem powiedzieć?\n");
    }
    else say(tstr, true);
}

static void sertempo()
{
    int t = getUInt(false, 0, 15);
    if (t < -1) return;
    if (t < 0) {
        printf("Tempo %d\n", pfs_tempo);
    }
    else {
        pfsSetTempo(t);
        printf("Ustawione\n");
    }
}

static void serblu()
{
    char *name = getToken(false);
    if (!name) {
        printf("Nazwa bluetooth: %s\n", pfs_btname);
        return;
    }
    if (*tstr) {
        printf("Błąd: nazwa zwiera spację\n");
        return;
    }
    if (strlen(name) < 3 || strlen(name) > 15) {
        printf("Błąd: nazwa może mieć od 3 do 15 znaków\n");
        return;
    }
    char *c=name;
    for (c=name;*c;c++) if (!isalnum(*c) && *c != '_' && *c != '-') {
        printf("Błąd: niedozwolony znak\n");
        return;
    }
    strcpy(pfs_btname, name);
    preferences.putString("bt", pfs_btname);
    printf("Ustawione\n");
}

static void serreset()
{
    char *t=getToken(false);
    extena = (t && *t == 'e') ? 1 : 0;
    ESP.restart();
}

static void sershmac()
{
    printf("Adres MAC %s\n",WiFi.macAddress().c_str());
}



static void sermac()
{
    char *t=getToken(false);
    if (!t) {
        if (!pfs_have_enmac) printf("Adres MAC metrówki nieustawiony\n");
        else printf("Adres MAC metrówki: %02X:%02X:%02X:%02X:%02X:%02X\n",
            enmac[0], enmac[1], enmac[2], enmac[3], enmac[4], enmac[5]);
        return;
    }
    uint8_t xmac[6];
    errno = 0;
    int i;
    for (i = 0; i<6;i++) {
        if (i && *t++!=':') break;
        xmac[i] = strtol(t,&t,16);
        if (errno) break;
    }
    if (i < 6) {
        printf("Błędny adres MAC\n");
    }
    else {
        memcpy(enmac,xmac,6);
        preferences.putBytes("enmacspkr",enmac, 6);
        printf("Adres MAC metrówki: %02X:%02X:%02X:%02X:%02X:%02X, zresetuj urządzenie\n",
            enmac[0], enmac[1], enmac[2], enmac[3], enmac[4], enmac[5]);
        
    }
}

const struct syndef {
    const char *name;
    uint8_t synth;
} syndefs[] = {
    {"Brak",ESYN_NONE},
    {"Bluetooth App", ESYN_BTA},
    {"BT Classic Terminal", ESYN_BTT},
    {"BLE Terminal", ESYN_BLT},
    {"Esp-now Metrówka", ESYN_METR},
    {NULL, 0}};
    

void sersynth()
{
    char *t=getToken(false);
    int i;
    if (!t) {
        for (i=0; syndefs[i].name; i++) if (syndefs[i].synth == pfs_extsynth) break;
        if (!syndefs[i].synth) i=0;
        printf("Zewnętrzny syntezator: %s\n", syndefs[i].name);
        return;
    }
    for (i=0; syndefs[i].name; i++) if (*t == syndefs[i].synth) {
        preferences.putInt("extsynth", syndefs[i].synth);
        printf("Ustawiono: %s. Zresetuj urządznie\n", syndefs[i].name);
        return;
    }
    printf("Błędny syntezator\n");
}

void sermsec()
{
    int t=getUInt(false, 1000, 3000);
    if (t<0) {
        if (t==-1) printf("Czas czytania %d msec\n", pfs_emillis);
        return;
    }
    pfs_emillis=t;
    preferences.putInt("extmillis",pfs_emillis);
    printf("Ustawione\n");
}

void servolt()
{
    int t = getUInt(false, 3000, 4500);
    if (t < 0) {
        if (t == -1) printf("Napięcie akumulatora %d mV, %d%%\n",
            (int)(rawVolt * pfs_mvmpx),
            batpercent());
        return;
    }
    pfs_mvmpx = (float)t/(float)rawVolt;
    preferences.putFloat("volt", pfs_mvmpx);
    printf("Woltomierz skalibrowany\n");
}

static void serlong()
{
    int t=getUInt(false, 300, 1000);
    if (t < 0) {
        if (t == -1) printf("Czas %d msec\n", pfs_longMsec);
        return;
    }
    pfs_longMsec = t;
    preferences.putInt("lmsec", t);
    printf("Ustawione\n");
}

static void serdouble()
{
    int t=getUInt(false, 200, 500);
    if (t < 0) {
        if (t == -1) printf("Czas %d msec\n", pfs_doubleMsec);
        return;
    }
    pfs_doubleMsec = t;
    preferences.putInt("dmsec", t);
    printf("Ustawione\n");
}

    
static void helpme(void);
static struct serialCmd {
    const char *name;
    void (*fun)(void);
    const char *descr;
    const char *params;
    const char *extras;
    
} commands[]={
    {"help", helpme, "Wyświetla pomoc dla podanego polecenia lub wszystkie rozpoznawane polecenia"},
    {"say", sersayme, "Przekazuje parametr do syntezatora mowy","<dowolny napis>"},
    {"tempo", sertempo,"Podaje lub ustawia tempo mowy","[liczba od 0 do 15]"},
    {"double",serdouble,"Podaje lub ustawia czas podwójnego kliknięcia","[czas w msec od 200 do 500]",
        "Po upływie tego czasu jeśli nie nastąpiło powtórne naciśnięcie przycisku\nurządzenie uznaje, że nastąpiło pojedyncze kliknięcie"},
        
    {"long",serlong,"Podaje lub ustawia czas przytrzymania przycisku","[czas w msec od 300 do 1000]",
        "Puszczenie przycisku przed upływem tego czasu\nuznane jest za krótkie kliknięcie"},
        
    {"synth", sersynth,"Podaje lub ustawia zewnętrzny syntezator","[litera n, b, t, l, m]",
            "n: brak\nb: bluetooth app\nt: bluetooth terminal\nl: ble terminal\nm: metrówka"},
    {"blut",serblu,"Podaje lub ustawia nazwę dla BlueTooth","[nazwa bez spacji od 3 do 15 znaków]"},
    {"mymac",sershmac,"Podaje adres MAC urządzenia"},
    {"mac",sermac,"Podaje lub ustawia adres MAC metrówki","[adres MAC]"},
    {"msec",sermsec,"Podaje lub ustawia czas czytania zewnętrznego syntezatora","[czas w msec od 1000 do 3000]"},
    {"volt",servolt,"Podaje napięcie akumulatora lub kalibruje woltomierz","[napięcie zmierzone w mV od 3000 do 4500]",
        "Aby skalibrować woltomierz, zmierz napięcie na gnieździe akumulatora\ni podaj jego rzeczywistą wartość w mV"},
    {"reset", serreset,"Restart mikrokontrolera","[e] - włącz zewnętrzny syntezator"},

    {NULL, NULL, NULL}};

static void helpme(void)
{
    int i;
    printf("\n");
    char *cmd=getToken(false);
    if (cmd) {
        for (i=0; commands[i].name; i++) if (!strcmp(cmd, commands[i].name)) {
            if (commands[i].params) {
                printf("%s %s: %s\n",commands[i].name, commands[i].params, commands[i].descr);
            }
            else {
                printf("%s: %s\n", commands[i].name, commands[i].descr);
            }
            if (commands[i].extras) printf("%s\n", commands[i].extras);
            return;
        }
        printf("Nierozpoznane polecenie %s\n", cmd);
        return;
    }
                
    printf("Polecenia:\n");
    for (i=0; commands[i].name; i++) {
        if (commands[i].params) {
                printf("%s %s: %s\n",commands[i].name, commands[i].params, commands[i].descr);
        }
        else {
            printf("%s: %s\n", commands[i].name, commands[i].descr);
        }
        
        
    }
    printf("---\n");
    return;
}


bool serloop()
{
    if (!getser()) return false;
    tstr = buf;
    char *cmd = getToken(false);
    if (!*cmd) return false;
    int i;
    for (i=0; commands[i].name; i++) {
        if (!strcmp(commands[i].name, cmd)) break;
    }
    if (commands[i].fun) commands[i].fun();
    else printf("Nierozpoznane polecenie '%s', wpisz \"help\" abty poznać polecenia\n", cmd);
    return true;
}

    
