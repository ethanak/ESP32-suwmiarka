#ifndef _E_PREFS_H
#define _E_PREFS_H 1
extern char pfs_btname[16];
extern int pfs_tempo;
extern int pfs_extsynth;
extern uint8_t enmac[6];
extern bool pfs_have_enmac;
extern int pfs_emillis;
extern bool pfs_batt;
extern float pfs_mvmpx;
extern uint32_t pfs_doubleMsec, pfs_longMsec;

extern void initPrefs();
extern bool serloop();
extern RTC_NOINIT_ATTR int extena;

extern void initBLE();
extern bool sayBLE(const char *c);
extern void loopBLE();


#define ESYN_NONE 'n'
#define ESYN_BTA 'b'
#define ESYN_BTT 't'
#define ESYN_BLT 'l'
#define ESYN_METR 'm'
//24:0A:C4:5A:AD:F0

#endif

