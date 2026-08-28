#ifndef API_H
#define API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// enum ApiEventDataType/struct ApiEventData live in kfx_config's
// script_hooks.h -- kfx_sim's actionpt.c is the lowest-ranked real
// producer of event data payloads and can't reach up to this (kfx_script)
// header, so the type is defined there and just used here.
#include "script_hooks.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int api_init_server();
    void api_update_server();
    void api_close_server();

    void api_event(const char *event_name);
    void api_event_with_data(const char *event_name, const struct ApiEventData *data, size_t data_count);

#ifdef __cplusplus
}
#endif

#endif