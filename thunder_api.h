#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern void addNewIpToMulticast();
// extern bool isAppRunning(const char *callsign);
int activateApp(const char* appName, const char* url);
int deActivateApp(const char* appName);
int listenIpChange();
bool getDialName(char* name, char* ret);
const char* getAppStatus(const char* callsign);
extern bool _appHidden;
bool hideApp(const char* callsign);
bool resumeApp(const char* callsign);

#ifdef __cplusplus
}
#endif