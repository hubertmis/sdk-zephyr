/*
 *  Copyright (c) 2024 Nordic Semiconductor ASA
 *
 *  SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "../include/sxsymcrypt/trng.h"
#include <cracen/statuscodes.h>
#include "crypmasterregs.h"
#include "hw.h"
#include "ba431regs.h"
#include "cmdma.h"
#include <security/cracen.h>

#include <zephyr/drivers/entropy.h>

#define DT_DRV_COMPAT nordic_entropy_cracen

#define RNG_CLKDIV		  (0)
#define RNG_OFF_TIMER_VAL	  (0)
#define RNG_INIT_WAIT_VAL	  (512)
#define RNG_NB_128BIT_BLOCKS	  (4)
#define RNG_FIFOLEVEL_GRANULARITY (4)
#define RNG_NO_OF_COND_KEYS	  (4)

static int ba431_check_state(void)
{
	uint32_t state = sx_rd_trng(BA431_REG_Status_OFST);

	state = (state & BA431_FLD_Status_State_MASK) >> BA431_FLD_Status_State_LSB;

	if (state == BA431_STATE_ERROR) {
		return SX_ERR_RESET_NEEDED;
	}

	if ((state == BA431_STATE_RESET) || (state == BA431_STATE_STARTUP)) {
		return SX_ERR_HW_PROCESSING;
	}

	return SX_OK;
}

static void ba431_softreset(void)
{
	uint32_t ctrl_reg = sx_rd_trng(BA431_REG_Control_OFST);

	sx_wr_trng(BA431_REG_Control_OFST, ctrl_reg | BA431_FLD_Control_SoftRst_MASK);
	sx_wr_trng(BA431_REG_Control_OFST, ctrl_reg & ~BA431_FLD_Control_SoftRst_MASK);
}

int sx_trng_open(struct sx_trng *ctx, const struct sx_trng_config *config)
{
	int err;
	*ctx = (struct sx_trng){0};

	cracen_acquire();
	ctx->initialized = true;

	/* Clear the control reg first */
	sx_wr_trng(BA431_REG_Control_OFST, 0x00);

	ba431_softreset();

	uint32_t fifo_wakeup_level;
	uint32_t rng_off_timer_val = RNG_OFF_TIMER_VAL;
	uint32_t rng_clkdiv = RNG_CLKDIV;
	uint32_t rng_init_wait_val = RNG_INIT_WAIT_VAL;
	uint32_t ctrlbitmask = 0;

	fifo_wakeup_level = sx_rd_trng(BA431_REG_FIFODepth_OFST) / RNG_FIFOLEVEL_GRANULARITY - 1;
	if (config) {
		if (config->wakeup_level) {
			if (config->wakeup_level > fifo_wakeup_level) {
				err = SX_ERR_TOO_BIG;
				goto error;
			}
			fifo_wakeup_level = config->wakeup_level;
		}
		if (config->off_time_delay) {
			rng_off_timer_val = config->off_time_delay;
		}
		if (config->init_wait) {
			rng_init_wait_val = config->init_wait;
		}
		if (config->sample_clock_div) {
			rng_clkdiv = config->sample_clock_div;
		}
		ctrlbitmask = config->control_bitmask;
	}

	/* Program powerdown and clock settings */
	sx_wr_trng(BA431_REG_FIFOThreshold_OFST, fifo_wakeup_level);
	sx_wr_trng(BA431_REG_SwOffTmrVal_OFST, rng_off_timer_val);
	sx_wr_trng(BA431_REG_ClkDiv_OFST, rng_clkdiv);
	sx_wr_trng(BA431_REG_InitWaitVal_OFST, rng_init_wait_val);

	/* Configure the control register */
	uint32_t control = sx_rd_trng(BA431_REG_Control_OFST);

	control &= ~(BA431_FLD_Control_Nb128BitBlocks_MASK | BA431_FLD_Control_ForceActiveROs_MASK |
		     BA431_FLD_Control_HealthTestBypass_MASK | BA431_FLD_Control_AIS31Bypass_MASK);
	control |= (RNG_NB_128BIT_BLOCKS << BA431_FLD_Control_Nb128BitBlocks_LSB) | ctrlbitmask;
	sx_wr_trng(BA431_REG_Control_OFST, control);

	/* Enable NDRNG */
	sx_wr_trng(BA431_REG_Control_OFST, (control & ~BA431_FLD_Control_Enable_MASK) | 0x01);

	return SX_OK;

error:
	cracen_release();
	ctx->initialized = false;
	return err;
}

static int ba431_setup_conditioning_key(struct sx_trng *ctx)
{
	uint32_t key;
	uint32_t level = sx_rd_trng(BA431_REG_FIFOLevel_OFST);

	/* FIFO level must be 4 (4 * 32bit Word) */
	if (level < RNG_NO_OF_COND_KEYS) {
		return SX_ERR_HW_PROCESSING;
	}

	for (size_t i = 0; i < RNG_NO_OF_COND_KEYS; i++) {
		key = sx_rd_trng(BA431_REG_FIFODATA_OFST);
		sx_wr_trng(BA431_REG_Key0_OFST + i * sizeof(key), key);
	}

	ctx->conditioning_key_set = 1;

	return SX_OK;
}

int sx_trng_get(struct sx_trng *ctx, char *dst, size_t size)
{
	int status = SX_OK;

	if (!ctx->initialized) {
		return SX_ERR_UNINITIALIZED_OBJ;
	}

	status = ba431_check_state();
	if (status) {
		return status;
	}

	/* Program random key for the conditioning function */
	if (!ctx->conditioning_key_set) {
		status = ba431_setup_conditioning_key(ctx);
		if (status != SX_OK) {
			return status;
		}
	}

	/* Block sizes above the FIFO wakeup level to guarantee that the
	 * hardware will be able at some time to provide the requested bytes.
	 */
	uint32_t wakeup_level = sx_rd_trng(BA431_REG_FIFOThreshold_OFST);

	if (size > (wakeup_level + 1) * 16) {
		return SX_ERR_TOO_BIG;
	}

	uint32_t level = sx_rd_trng(BA431_REG_FIFOLevel_OFST);
	/* FIFO level in 4-byte words */
	if (size > level * RNG_FIFOLEVEL_GRANULARITY) {
		return SX_ERR_HW_PROCESSING;
	}

	while (size) {
		uint32_t data;

		data = sx_rd_trng(BA431_REG_FIFODATA_OFST);
		for (size_t i = 0; (i < sizeof(data)) && (size); i++, size--) {
			*dst = (char)(data & 0xFF);
			dst++;
			data >>= 8;
		}
	}

	return status;
}

int sx_trng_close(struct sx_trng *ctx)
{
	if (ctx->initialized) {
		cracen_release();
		ctx->initialized = false;
	}
	return SX_OK;
}

static int get_entropy(const struct device *dev, uint8_t *buffer, uint16_t length)
{
	ARG_UNUSED(dev);

    struct sx_trng trng;
    int err;

    err = sx_trng_open(&trng, NULL);
    if (err != SX_OK) {
		printk("Failed to open TRNG: %d\n", err);
        return err;
    }

	do {
		err = sx_trng_get(&trng, buffer, length);
	} while (err == SX_ERR_HW_PROCESSING);

	if (err != SX_OK) {
		printk("Failed to get TRNG: %d\n", err);
	}
    (void)sx_trng_close(&trng);

    return err;
}

static int get_entropy_isr(const struct device *dev, uint8_t *buf, uint16_t len,
					uint32_t flags)
{
    // TODO: Implement. Currently mutex in the CRACEN driver cannot be used. Would need to be removed.
    return -ENOTSUP;
}

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);
    return 0;
}

static const struct entropy_driver_api entropy_cracen_api = {
	.get_entropy = get_entropy, .get_entropy_isr = get_entropy_isr};

DEVICE_DT_INST_DEFINE(0, init, NULL, NULL, NULL, PRE_KERNEL_1,
		      CONFIG_ENTROPY_INIT_PRIORITY, &entropy_cracen_api);
