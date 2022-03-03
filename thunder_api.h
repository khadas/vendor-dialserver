#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "dial_server.h"
extern void addNewIpToMulticast();
extern int ssdp_running;
int activateApp(const char* callsign, const char* url);
int deActivateApp(const char* callsign);
int listenIpChange();
bool getDialName(const char* name, char* ret);
DIALStatus getAppStatus(const char* callsign);
extern bool _appHidden;
bool hideApp(const char* callsign);
bool resumeApp(const char* callsign);
int loadJson(const char* jsonFile);

struct appInfo {
    char* name;
    char* handler;
    char* callsign;
    char* hide;
    char* url;
    struct appInfo *next;
};
extern struct appInfo* appInfoList;
extern struct appInfo* curAppForDial;

#ifdef __cplusplus
}
#endif