#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "enow.h"
#include "prefs.h"

/* 24:0A:C4:C5:89:40*/

extern uint8_t speaking;
static volatile int8_t xstatus,enow_speaking, enow_rcvd;
static uint8_t keyen[16]={1,15,24,51,111,204,13,6,11,199,212,213,215,1,43,41};
static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
/*
  printf("\r\nLast Packet Send Status for %02x:%02x:%02x:%02x:%02x:%02x",
    mac_addr[0],
    mac_addr[1],
    mac_addr[2],
    mac_addr[3],
    mac_addr[4],
    mac_addr[5]
    );
  Serial.println((status == ESP_NOW_SEND_SUCCESS) ? "Delivery Success" : "Delivery Fail");
  */
  xstatus = (status == ESP_NOW_SEND_SUCCESS)?1:0;
  //printf("X-status = %d\n", xstatus);
}

static uint32_t enow_timer;

static void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
    /*printf("Received from %02x:%02x:%02x:%02x:%02x:%02x %-*.*s\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len, len,(char *)incomingData);*/
    if (len == 5) {
        if (!strncmp((char *)incomingData,"fin ",4)) enow_rcvd=incomingData[4];
    }
}

esp_now_peer_info_t peerInfo={0};
    
bool initENow(void)
{
    if (!pfs_have_enmac) return false;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) return false;
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
    memcpy(peerInfo.peer_addr, enmac, 6);
    peerInfo.channel = 1;//pfs_enchannel;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk,keyen,16);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
    //printf("EN Initialized\n");
    return true;
}

static char enext;
static char nextEN()
{
    if (enext < 'a' || enext >='z') {
        enext='a';
    }
    else {
        enext++;
    }
    return enext;
        
}


bool sayENow(const char *c)
{
    enow_speaking=0;
    enow_rcvd=0;
    if (strlen(c) > 200) return false;
    char buf[210];
    nextEN();
    sprintf(buf,"say %c%s", enext, c);
    xstatus = -1;
    if (esp_now_send(enmac, (uint8_t *) buf, strlen(buf)) != ESP_OK) {
        //printf("Not send\n");
        return false;
    }
    for (int i=0; i<10; i++) {
        if (i) delay(10);
        if (xstatus == 0) return false;
        else if (xstatus == 1) {
            speaking = 1;
            enow_speaking=enext;
            enow_timer= millis();
            return true;
        }
        
    }
    
    return false;
    
}

void stopENow(void)
{
    sayENow("");
}

extern uint32_t lastSpoken;
 bool loopENow(void)
{
    
    if (enow_speaking) {
        if (millis() - enow_timer > 3000 || enow_rcvd == enow_speaking)  {
            enow_speaking = 0;
            enow_rcvd=0;
            speaking = 0;
            lastSpoken = millis();
        }
        return true;
    }
    return false;
}

