#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include "url_lib.h"
#include "amazon_callbacks.h"

DIALStatus amazon_start(DIALServer *ds, const char *appname,
                                const char *payload, const char* query_string,
                                const char *additionalDataUrl,
                                DIAL_run_t *run_id, void *callback_data) {
    const char url[4] = { 0 };
    matchAppInfo(appname);
    if (activateApp(curAppForDial->callsign, url) == 0)
        return kDIALStatusRunning;
    return kDIALStatusStopped;
}

DIALStatus amazon_hide(DIALServer *ds, const char *app_name,
                               DIAL_run_t *run_id, void *callback_data)
{
    matchAppInfo(app_name);
    return hideApp(curAppForDial->callsign) ? kDIALStatusHide : kDIALStatusError;
}

DIALStatus amazon_status(DIALServer *ds, const char *appname,
                                 DIAL_run_t run_id, int* pCanStop, void *callback_data) {
    if (pCanStop) *pCanStop = 1;
    matchAppInfo(appname);
    return getAppStatus(curAppForDial->callsign);
}

void amazon_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                         void *callback_data) {
    printf("\n\n ** KILL Amazon: %s **\n\n", appname);
    matchAppInfo(appname);
    deActivateApp(curAppForDial->callsign);
}