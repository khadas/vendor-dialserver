#ifndef _AMAZON_CALLBACKS_H
#define _AMAZON_CALLBACKS_H

#include <stdbool.h>
#include "thunder_api.h"

DIALStatus amazon_start(DIALServer *ds, const char *appname,
                            const char *payload, const char* query_string,
                            const char *additionalDataUrl,
                            DIAL_run_t *run_id, void *callback_data);

DIALStatus amazon_hide(DIALServer *ds, const char *app_name,
                        DIAL_run_t *run_id, void *callback_data);

DIALStatus amazon_status(DIALServer *ds, const char *appname,
                          DIAL_run_t run_id, int* pCanStop, void *callback_data);

void amazon_stop(DIALServer *ds, const char *appname, DIAL_run_t run_id,
                  void *callback_data);

#endif