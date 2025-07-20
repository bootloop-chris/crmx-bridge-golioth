/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <golioth/client.h>
#include "golioth_nvs.h"
#include "device_config.h"

#ifndef CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS
#include "golioth_nvs.h"
#endif

#define TAG "golioth_credentials"

static struct golioth_client_config _golioth_client_config = {
    .credentials =
        {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK,
            .psk =
                {
                    .psk_id = NULL,
                    .psk_id_len = 0,
                    .psk = NULL,
                    .psk_len = 0,
                },
        },
};

const struct golioth_client_config *golioth_sample_credentials_get(void)
{
#ifdef CONFIG_GOLIOTH_SAMPLE_HARDCODED_CREDENTIALS

    _golioth_client_config.credentials.psk.psk_id = CONFIG_GOLIOTH_SAMPLE_PSK_ID;
    _golioth_client_config.credentials.psk.psk_id_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK_ID);
    _golioth_client_config.credentials.psk.psk = CONFIG_GOLIOTH_SAMPLE_PSK;
    _golioth_client_config.credentials.psk.psk_len = strlen(CONFIG_GOLIOTH_SAMPLE_PSK);

#else

    const char *psk_id = nvs_read_golioth_psk_id();
    const char *psk = nvs_read_golioth_psk();
    
    // Try defaults from device_config.h if NVS doesn't have them
    if (strcmp(psk_id, NVS_DEFAULT_STR) == 0 && strlen(DEFAULT_GOLIOTH_PSK_ID) > 0) {
        psk_id = DEFAULT_GOLIOTH_PSK_ID;
    }
    if (strcmp(psk, NVS_DEFAULT_STR) == 0 && strlen(DEFAULT_GOLIOTH_PSK) > 0) {
        psk = DEFAULT_GOLIOTH_PSK;
    }
    
    _golioth_client_config.credentials.psk.psk_id = psk_id;
    _golioth_client_config.credentials.psk.psk_id_len = strlen(psk_id);
    _golioth_client_config.credentials.psk.psk = psk;
    _golioth_client_config.credentials.psk.psk_len = strlen(psk);
#endif

    return &_golioth_client_config;
}