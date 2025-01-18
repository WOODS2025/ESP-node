/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef GAP_SVC_H
#define GAP_SVC_H

/* Includes */
/* NimBLE GAP APIs */
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"

/* Defines */
#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00

typedef enum {
    NODE_STATE_TRANSMIT,
    NODE_STATE_LISTEN,
    NODE_STATE_LOUD,
} node_state;

/* Public function declarations */
void adv_init(void);
bool is_connection_encrypted(uint16_t conn_handle);
int gap_init(void);
extern node_state state;

#endif // GAP_SVC_H
