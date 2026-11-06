/*
 * w5500_port.h
 *
 *  Created on: Nov 6, 2026
 *      Author: Rubin Khadka
 */

#ifndef ETHERNET_W5500_W5500_PORT_H_
#define ETHERNET_W5500_W5500_PORT_H_

#include <stdint.h>
#include "wizchip_conf.h"

typedef enum {
    W5500_OK        =  0,
    W5500_ERR_INIT  = -1,   /* ctlwizchip(CW_INIT_WIZCHIP) failed */
    W5500_ERR_COMM  = -2,   /* version register didn't read 0x04 */
    W5500_ERR_LINK  = -3    /* no Ethernet link detected */
} W5500_Status_t;

W5500_Status_t W5500_Init(void);

const wiz_NetInfo *W5500_GetNetInfo(void);

#endif /* ETHERNET_W5500_W5500_PORT_H_ */
