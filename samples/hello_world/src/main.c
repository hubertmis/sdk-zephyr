/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <mac_features/ack_generator/nrf_802154_ack_generator.h>
#include <mac_features/ack_generator/nrf_802154_ack_generator.h>


static void cyccnt_enable(void)
{
    uint32_t * scb_demcr = (uint32_t *)0xE000EDFC;
    uint32_t * dwt_ctrl  = (uint32_t *) 0xE0001000;

    *scb_demcr |= 0x01000000;
    *dwt_ctrl  |= 0x00000001;
}

static uint32_t cyccnt_get(void)
{
    uint32_t * dwt_cyccnt = (uint32_t *)0xE0001004;
    return *dwt_cyccnt;
}

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

    cyccnt_enable();

    uint8_t frame[] = {0x10,
        0x41, 0xa8, 0xaa, 0xcd, 0xab, 0x34, 0x12, 0x45,
        0x23, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd,
    };
    nrf_802154_frame_parser_data_t fp_data;

    bool parse_result = nrf_802154_frame_parser_data_init(frame, frame[0], PARSE_LEVEL_AUX_SEC_HDR_END, &fp_data);

    uint32_t start_time = cyccnt_get();
    
    const uint8_t * ack = nrf_802154_ack_generator_create(&fp_data);

    uint32_t stop_time = cyccnt_get();

    printk("Parse result: %d\n", (int)parse_result);
	printk("Generated ACK: %x\n", (int)ack);
    printk("Time: %u ... %u = %u\n", start_time, stop_time, stop_time - start_time);
}
