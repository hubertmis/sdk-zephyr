/*
 * Copyright 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/mspi.h>

#define MSPI_BUS                  DT_BUS(DT_ALIAS(dev0))
#define MSPI_TARGET               DT_ALIAS(dev0)

#define BUF_SIZE 1024

const uint8_t one_wire_cmd = 0x02;
const uint32_t one_wire_address = 0xaa5599;
uint8_t one_wire_params[] = {0x5a, 0xdb};

const uint8_t data_block_cmd = 0x32;
const uint32_t data_block_address = 0x002c00;
uint8_t data_block_data[256];

const struct mspi_xfer_packet one_wire_packet[] = {
	{
		.dir                = MSPI_TX,
		.cmd                = one_wire_cmd,
		.address            = one_wire_address,
		.num_bytes          = sizeof(one_wire_params),
		.data_buf           = one_wire_params,
	},
};

const struct mspi_xfer_packet data_block_packet[] = {
	{
		.dir                = MSPI_TX,
		.cmd                = data_block_cmd,
		.address            = data_block_address,
		.num_bytes          = sizeof(data_block_data),
		.data_buf           = data_block_data,
	},
};

const struct mspi_xfer one_wire_xfer = {
	.xfer_mode                  = MSPI_DMA,
	.cmd_length                 = 1,
	.addr_length                = 3,
	.priority                   = 1,
	.packets                    = one_wire_packet,
	.num_packet                 = 1,
};

const struct mspi_xfer data_block_xfer = {
	.xfer_mode                  = MSPI_DMA,
	.cmd_length                 = 1,
	.addr_length                = 3,
	.priority                   = 1,
	.packets                    = data_block_packet,
	.num_packet                 = 1,
};

int main(void)
{
	const struct device *controller = DEVICE_DT_GET(MSPI_BUS);
	struct mspi_dev_id dev_id = MSPI_DEVICE_ID_DT(MSPI_TARGET);
	int ret;

	/* Initialize write buffer */
	for (int i = 0; i < ARRAY_SIZE(data_block_data); i++) {
		data_block_data[i] = (uint8_t)i;
	}

	// TODO: cache should be handled by the driver. Is this code needed here?
	ret = sys_cache_data_flush_range(data_block_data, sizeof(data_block_data));
	if (ret) {
		printk("Failed to flush cache\n");
		return 1;
	}

	const struct mspi_dev_cfg one_wire_cfg = {
		.freq = 32000000,
		.io_mode = MSPI_IO_MODE_SINGLE,
	};
	ret = mspi_dev_config(controller, &dev_id,
	                      MSPI_DEVICE_CONFIG_FREQUENCY |
						  MSPI_DEVICE_CONFIG_IO_MODE,
						  &one_wire_cfg);
	if (ret) {
		printk("Failed to configure mode\n");
		return 1;
	}

	ret = mspi_transceive(controller, &dev_id, &one_wire_xfer);
	if (ret) {
		printk("Failed to send configuration\n");
		return 1;
	}

	const struct mspi_dev_cfg data_block_cfg = {
		.freq = 50000000,
		.io_mode = MSPI_IO_MODE_QUAD_1_1_4,
	};
	ret = mspi_dev_config(controller, &dev_id,
	                      MSPI_DEVICE_CONFIG_FREQUENCY |
						  MSPI_DEVICE_CONFIG_IO_MODE,
						  &data_block_cfg);
	if (ret) {
		printk("Failed to configure 1 1 4 mode\n");
		return 1;
	}

	ret = mspi_transceive(controller, &dev_id, &data_block_xfer);
	if (ret) {
		printk("Failed to send data\n");
		return 1;
	}

	printk("MSPI test completed\n");

	return 0;
}
