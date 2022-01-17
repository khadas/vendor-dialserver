/*
 * Copyright (c) 2014-2020 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETFLIX, INC. AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NETFLIX OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>

#include "dial_server.h"
#include "dial_options.h"
#include <signal.h>
#include <stdbool.h>

#include "url_lib.h"
#include "nf_callbacks.h"
#include "system_callbacks.h"

#include "thunder_api.h"
#include <net/if.h>
#include <sys/ioctl.h>

#define BUFSIZE 256

static char spFriendlyName[BUFSIZE];
static char spModelName[BUFSIZE];
static char spUuid[BUFSIZE] = "deadbeef-wlfy-beef-dead-";
extern bool wakeOnWifiLan;  //set the value to true to support WoL/WoWLAN in main(), false to nonosupport;default is true.
static int gDialPort;
static char *spDefaultUuid = "deadbeef-wlfy-beef-dead-beefdeadbeef";
bool _appHidden = false;

//used to netflix start function.
char *spAppNetflix = "netflix";      // name of the netflix executable
static char *spDefaultNetflix = "../../../src/platform/qt/netflix";
char spNetflix[BUFSIZE];
//set the value of spDataDir,and putenv(spDataDir) in old runApplication() function.
static char spDataDir[BUFSIZE];
static char *spNfDataDir = "NF_DATA_DIR=";
static char *spDefaultData="../../../src/platform/qt/data";
//used to system callback function.
char spSleepPassword[BUFSIZE]; 

extern int ssdp_running;
void signalHandler(int signal)
{
    fprintf(stderr, "signal %d\n", signal);
    switch(signal)
    {
        case SIGTERM:
            // just ignore this, we don't want to die
            break;
    }
    ssdp_running = 0;
}

/*
 * This function will walk /proc and look for the application in
 * /proc/<PID>/comm. and /proc/<PID>/cmdline to find it's command (executable
 * name) and command line (if needed).
 * Implementors can override this function with an equivalent.
 */
DIALStatus appStatus(const char *callsign) {
    if(strcmp(getAppStatus(callsign),"resumed")  == 0) {
        printf("amldial-appStatus %s\n",getAppStatus(callsign));
        return kDIALStatusRunning;
    }
    else if(strcmp(getAppStatus(callsign),"suspended")  == 0) {
        printf("amldial-appStatus %s\n",getAppStatus(callsign));
        return kDIALStatusHide;
    }
    else if(strcmp(getAppStatus(callsign),"deactivated")  == 0) {
        printf("amldial-appStatus %s\n",getAppStatus(callsign));
        return kDIALStatusStopped;
    }
    else return kDIALStatusError;
}

/* Compare the applications last launch parameters with the new parameters.
 * If they match, return false
 * If they don't match, return true
 */
int shouldRelaunch(
    DIALServer *pServer,
    const char *pAppName,
    const char *args )
{
    return ( strncmp( DIAL_get_payload(pServer, pAppName), args, DIAL_MAX_PAYLOAD ) != 0 );
}

static DIALStatus youtube_start(DIALServer *ds, const char *appname,
                                const char *payload, const char* query_string,
                                const char *additionalDataUrl,
                                DIAL_run_t *run_id, void *callback_data) {
    printf("\n\n ** LAUNCH YouTube ** with payload %s\n\n", payload);

    char url[512] = {0,};
    if (strlen(payload) && strlen(additionalDataUrl)) {
        snprintf( url, sizeof(url), "https://www.youtube.com/tv?%s&%s&inApp=true", payload, additionalDataUrl);
    } else if (strlen(payload)) {
        snprintf( url, sizeof(url), "https://www.youtube.com/tv?%s&inApp=true", payload);
    } else {
      snprintf( url, sizeof(url), "https://www.youtube.com/tv&inApp=true");
    }

    if(activateApp("youtube",url) == 0)
        return kDIALStatusRunning;
    return kDIALStatusStopped;;
}

static DIALStatus youtube_hide(DIALServer *ds, const char *app_name,
                                        DIAL_run_t *run_id, void *callback_data) {
    return hideApp("Cobalt") ? kDIALStatusHide : kDIALStatusError;
}
        
static DIALStatus youtube_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int *pCanStop, void *callback_data) {
    if (pCanStop) *pCanStop = 1;
    appStatus("Cobalt");
}

static void youtube_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL YouTube **\n\n");
    deActivateApp("youtube");
}

void run_ssdp(int port, const char *pFriendlyName, const char * pModelName, const char *pUuid);

void runDial(void)
{
    DIALServer *ds;
    ds = DIAL_create();
    if (ds == NULL) {
        printf("Unable to create DIAL server.\n");
        return;
    }
    
    struct DIALAppCallbacks cb_nf;
    cb_nf.start_cb = netflix_start;
    cb_nf.hide_cb = netflix_hide;
    cb_nf.stop_cb = netflix_stop;
    cb_nf.status_cb = netflix_status;
    struct DIALAppCallbacks cb_yt = {youtube_start, youtube_hide, youtube_stop, youtube_status};
    struct DIALAppCallbacks cb_system = {system_start, system_hide, NULL, system_status};

#if defined DEBUG
    if (DIAL_register_app(ds, "Netflix", &cb_nf, NULL, 1, "https://netflix.com https://www.netflix.com https://port.netflix.com:123 proto://*") == -1 ||
        DIAL_register_app(ds, "YouTube", &cb_yt, NULL, 1, "https://youtube.com https://www.youtube.com https://*.youtube.com:443 https://port.youtube.com:123 package:com.google.android.youtube package:com.google.ios.youtube proto:*") == -1 ||
#else
    if (DIAL_register_app(ds, "Netflix", &cb_nf, NULL, 1, "https://netflix.com https://www.netflix.com") == -1 ||
        DIAL_register_app(ds, "YouTube", &cb_yt, NULL, 1, "https://youtube.com https://*.youtube.com package:*") == -1 ||
#endif
        DIAL_register_app(ds, "system", &cb_system, NULL, 1, "") == -1)
    {
        printf("Unable to register DIAL applications.\n");
    } else if (!DIAL_start(ds)) {
        printf("Unable to start DIAL master listening thread.\n");
    } else {
        gDialPort = DIAL_get_port(ds);
        printf("launcher listening on gDialPort %d\n", gDialPort);
        run_ssdp(gDialPort, spFriendlyName, spModelName, spUuid);
        
        DIAL_stop(ds);
    }
    free(ds);
}

static void setValue( char * pSource, char dest[] )
{
    // Destination is always one of our static buffers with size BUFSIZE
    memset( dest, 0, BUFSIZE );
    int length = (strlen(pSource) < BUFSIZE - 1) ? strlen(pSource) : (BUFSIZE - 1);
    memcpy( dest, pSource, length );
}

bool get_mac(char* mac, const char *if_typ)
{
    struct ifreq tmp;
    int sock_mac;
    char mac_addr[18];
    sock_mac = socket(AF_INET, SOCK_STREAM, 0);
    if( sock_mac == -1)
    {
        return false;
    }
    memset(&tmp,0,sizeof(tmp));
    strncpy(tmp.ifr_name, if_typ,sizeof(tmp.ifr_name)-1 );
    if( (ioctl( sock_mac, SIOCGIFHWADDR, &tmp)) < 0 )
    {
        close(sock_mac);
        return false;
    }
    sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x",
            (unsigned char)tmp.ifr_hwaddr.sa_data[0],
            (unsigned char)tmp.ifr_hwaddr.sa_data[1],
            (unsigned char)tmp.ifr_hwaddr.sa_data[2],
            (unsigned char)tmp.ifr_hwaddr.sa_data[3],
            (unsigned char)tmp.ifr_hwaddr.sa_data[4],
            (unsigned char)tmp.ifr_hwaddr.sa_data[5]
            );
    close(sock_mac);
    memcpy(mac,mac_addr,strlen(mac_addr));
    return true;
}

void setDialProperty() {
    char ret[128] = {0};
    char mac_addr[18] = {0};
    setValue(getDialName("DIALSERVER_NAME",ret) ? ret : "Platform-Amlogic-Defult",spFriendlyName);
    setValue(getDialName("MODEL_NAME",ret) ? ret : "OTT-Defult",spModelName);
    get_mac(mac_addr, "eth0") ? strcat(spUuid, mac_addr) : setValue( spDefaultUuid, spUuid );
}

int main(int argc, char* argv[])
{
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    srand(time(NULL));
    setDialProperty();

    listenIpChange();
    runDial();

    return 0;
}

