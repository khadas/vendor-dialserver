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
DIALStatus getAppStatus(const char* callsign);
extern bool _appHidden;
bool hideApp(const char* callsign);
bool resumeApp(const char* callsign);
int loadJson(const char* jsonFile);
void setDialProperty(char *friendlyname, char *uuid, char *modelname);
extern char friendly_name[256];
extern char uuid[256];
extern char model_name[256];

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