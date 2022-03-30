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
#define JSONFILEPATH "/etc/amldial/AMLDIAL.json"

static char spFriendlyName[BUFSIZE];
static char spModelName[BUFSIZE];
static char spUuid[BUFSIZE] = "deadbeef-wlfy-beef-dead-";
extern bool wakeOnWifiLan;  //set the value to true to support WoL/WoWLAN in main(), false to nonosupport;default is true.
static int gDialPort;
bool _appHidden = false;
struct appInfo* appInfoList = NULL;
struct appInfo* curAppForDial = NULL;

//set the value of spDataDir,and putenv(spDataDir) in old runApplication() function.(it is useless now)
static char spDataDir[BUFSIZE];
static char *spNfDataDir = "NF_DATA_DIR=";
static char *spDefaultData="../../../src/platform/qt/data";
//used to system(app) callback function.(it is useless now)
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

void matchAppInfo(const char *appname) {
    if (curAppForDial && !strcmp(curAppForDial->name,appname)) {
        //do nothing
    }
    else {
        //find the match appInfo
        struct appInfo* temp = appInfoList;
        while (temp->next) {
            temp = temp->next;
            if (!strcmp(temp->name,appname)) {
                curAppForDial = temp;
                break;
            }
        }
    }
}

static DIALStatus youtube_start(DIALServer *ds, const char *appname,
                                const char *payload, const char* query_string,
                                const char *additionalDataUrl,
                                DIAL_run_t *run_id, void *callback_data) {
    printf("\n\n ** LAUNCH YouTube: %s ** with payload %s\n\n", appname, payload);

    matchAppInfo(appname);
    char url[512] = {0,};
    if (strlen(payload) && strlen(additionalDataUrl)) {
        snprintf( url, sizeof(url), "%s?%s&%s&inApp=true", curAppForDial->url, additionalDataUrl, payload);
    } else if (strlen(payload)) {
        snprintf( url, sizeof(url), "%s?%s&inApp=true", curAppForDial->url, payload);
    } else {
      snprintf( url, sizeof(url), "%s&inApp=true", curAppForDial->url);
    }

    if(activateApp(curAppForDial->callsign, url) == 0)
        return kDIALStatusRunning;
    return kDIALStatusStopped;
}

static DIALStatus youtube_hide(DIALServer *ds, const char *app_name,
                                        DIAL_run_t *run_id, void *callback_data) {
    matchAppInfo(app_name);
    return hideApp(curAppForDial->callsign) ? kDIALStatusHide : kDIALStatusError;
}
        
static DIALStatus youtube_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int *pCanStop, void *callback_data) {
    if (pCanStop) *pCanStop = 1;
    matchAppInfo(appname);
    return getAppStatus(curAppForDial->callsign);
}

static void youtube_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL YouTube: %s **\n\n", appname);
    matchAppInfo(appname);
    deActivateApp(curAppForDial->callsign);
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

    struct appInfo* temp =  appInfoList;
    struct DIALAppCallbacks* appHandler;
    while(temp->next) {
        temp = temp->next;
        if (!strcmp(temp->handler, "YouTube")) {
            appHandler = &cb_yt;
            if (DIAL_register_app(ds, temp->name, appHandler, NULL, 1, "https://youtube.com https://*.youtube.com package:*") == -1) 
                printf("Unable to register DIAL application : %s.\n", temp->name);
        }
        else if (!strcmp(temp->handler, "Netflix")) {
            appHandler = &cb_nf;
            if (DIAL_register_app(ds, temp->name, appHandler, NULL, 1, "https://netflix.com https://www.netflix.com") == -1) 
                printf("Unable to register DIAL application : %s.\n", temp->name);
        }
        else continue;
    }

    if (!DIAL_start(ds)) {
        printf("Unable to start DIAL master listening thread.\n");
    } else {
        gDialPort = DIAL_get_port(ds);
        printf("launcher listening on gDialPort %d\n", gDialPort);
        run_ssdp(gDialPort, spFriendlyName, spModelName, spUuid);
        DIAL_stop(ds);
    }
    free(ds);
    //free the list
    struct appInfo* temp1 =  appInfoList -> next;
    struct appInfo* temp2;
    while (temp1) {
        temp2 = temp1 -> next;
        free(temp1->name);
        free(temp1->handler);
        free(temp1->callsign);
        free(temp1->hide);
        free(temp1->url);
        free(temp1);
        temp1 = temp2;
    }
    free(appInfoList);
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

    listenIpChange();
    loadJson(JSONFILEPATH);
    setDialProperty(spFriendlyName, spUuid, spModelName);
    runDial();

    return 0;
}

