/*
 * Copyright (c) 2014-2019 Netflix, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include "dial_server.h"
#include "url_lib.h"
#include "nf_callbacks.h"

static char *defaultLaunchParam = "source_type=12";

// Adding 40 bytes for defaultLaunchParam, plus additional characters that
// may get added.
//
// dial_server.c ensures the payload is <= DIAL_MAX_PAYLOAD but since we
// URL-encode it, the total string length may triple.
//
// dial_server.c ensures the additional data URL is <= DIAL_MAX_ADDITIONALURL.
static char sQueryParam[3 * DIAL_MAX_PAYLOAD + DIAL_MAX_ADDITIONALURL + 40];

DIALStatus netflix_start(DIALServer *ds, const char *appname,
                                const char *payload, const char* query_string,
                                const char *additionalDataUrl,
                                DIAL_run_t *run_id, void *callback_data) {
    memset( sQueryParam, 0, sizeof(sQueryParam) );
    strncpy( sQueryParam, defaultLaunchParam, sizeof(sQueryParam) - 1);
    if(strlen(payload))
    {
        char * pUrlEncodedParams;
        pUrlEncodedParams = url_encode( payload );
        if( pUrlEncodedParams ){
            if (strlen(sQueryParam) + sizeof("&dial=") + strlen(pUrlEncodedParams) < sizeof(sQueryParam)) {
                strcat( sQueryParam, "&dial=" );
                strcat( sQueryParam, pUrlEncodedParams );
                free( pUrlEncodedParams );
            } else {
                free( pUrlEncodedParams );
                return kDIALStatusError;
            }

        }
    }

    if(strlen(additionalDataUrl)){
        if (strlen(sQueryParam) + sizeof("&") + strlen(additionalDataUrl) < sizeof(sQueryParam)) {
            strcat(sQueryParam, "&");
            strcat(sQueryParam, additionalDataUrl);
        } else {
            return kDIALStatusError;
        }
    }
    matchAppInfo(appname);
    if(activateApp(curAppForDial->callsign, sQueryParam) == 0)
        return kDIALStatusRunning;
    return kDIALStatusStopped;
}

DIALStatus netflix_hide(DIALServer *ds, const char *app_name,
                               DIAL_run_t *run_id, void *callback_data)
{
    matchAppInfo(app_name);
    return hideApp(curAppForDial->callsign) ? kDIALStatusHide : kDIALStatusError;
}

DIALStatus netflix_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int* pCanStop, void *callback_data) {
    if (pCanStop) *pCanStop = 1;
    matchAppInfo(appname);
    return getAppStatus(curAppForDial->callsign);
}

void netflix_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL YouTube: %s **\n\n", appname);
    matchAppInfo(appname);
    deActivateApp(curAppForDial->callsign);
}


