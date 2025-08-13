/*
 * Copyright (c) 2020 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/misc/lorem_ipsum.h>
#include <string.h>
#include <stdlib.h>
#include "lz4frame.h"
#include "lz4.h"

#include "lz4_array.h"

int main(void)
{
    uint32_t decomp_start_time, decomp_end_time;
    size_t src_size = 0;
    static char decompressed_data[64 * 1024];
    char *src = NULL;

    decomp_start_time = k_cycle_get_32();

    const char *compressed_src = (const char *)zephyr_bin_lz4;
    size_t src_size_remaining = sizeof(zephyr_bin_lz4) / sizeof(zephyr_bin_lz4[0]);
    size_t src_offset = 0;
    int decompressed_size = 0;
    int total_decompressed_size = 0;

    /* Allocate memory for decompressed data */
    if (!decompressed_data) {
     printf("Failed to allocate memory for decompressed data\n");
     return 0;
    }

    /* Create LZ4F decompression context */
    void *dctx = NULL;
    size_t result = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);

    if (LZ4F_isError(result)) {
     printf("Failed to create LZ4F decompression context: %s\n",
      LZ4F_getErrorName(result));
     return 0;
    }

    /* Get frame info */
    LZ4F_frameInfo_t frameInfo;
    size_t consumed = src_size_remaining;
    result = LZ4F_getFrameInfo(dctx, &frameInfo, compressed_src, &consumed);

    if (LZ4F_isError(result)) {
     printf("Failed to get LZ4 frame info: %s\n",
      LZ4F_getErrorName(result));
     LZ4F_freeDecompressionContext(dctx);
     return 0;
    }

    src_offset += consumed;
    src_size_remaining -= consumed;

    /* Decompress the data in streaming mode, overwriting the output buffer each time */
    while (src_size_remaining > 0) {
     /* Reset output buffer size for this iteration */
     size_t dst_size = ARRAY_SIZE(decompressed_data);
     size_t src_size_to_process = src_size_remaining;

     /* Decompress a chunk */
     size_t ret = LZ4F_decompress(dctx,
            decompressed_data,
            &dst_size,
            compressed_src + src_offset,
            &src_size_to_process,
            NULL);

     if (LZ4F_isError(ret)) {
      printf("Decompression failed: %s\n", LZ4F_getErrorName(ret));
      LZ4F_freeDecompressionContext(dctx);
      return 0;
     }

     /* Update counters */
     src_offset += src_size_to_process;
     src_size_remaining -= src_size_to_process;
     total_decompressed_size += dst_size;

     /* If ret == 0, we've reached the end of the frame */
     if (ret == 0) {
      break;
     }
    }

    /* Clean up */
    LZ4F_freeDecompressionContext(dctx);

    /* For comparison purposes with the original code, set decompressed_size
       to the total size that has been decompressed */
    decompressed_size = total_decompressed_size;
    decomp_end_time = k_cycle_get_32(); /* Use k_cycle_get_32 instead of k_uptime_get_32 */

    if (decompressed_size < 0) {
     printf("Failed to decompress the data\n");
     return 0;
    }

    /* Convert cycles to microseconds using CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC */
    uint32_t decomp_time_us = (uint32_t)((decomp_end_time - decomp_start_time) *
                             (1000000.0 / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC));
    printf("Decompression took %u us\n", decomp_time_us);

    /* Remove free of compressed_data since it wasn't allocated in this code */

    if (decompressed_size >= 0) {
     printf("Successfully decompressed some data\n");
     /* Hexdump the last 8 bytes of decompressed data */
     printf("Last 8 bytes of decompressed data: ");
     for (int i = 0; i < 8 && (decompressed_size % (64 * 1024) - i) > 0; i++) {
         printf("%02x ", (unsigned char)decompressed_data[decompressed_size % (64 * 1024) - 8 + i]);
     }
     printf("\n");
    }

    if (decompressed_size != src_size) {
     printf("Decompressed data is different from original: %zu != %zu\n", decompressed_size, src_size);
     return 0;
    }

    if (memcmp(src, decompressed_data, src_size) != 0) {
     printf("Validation failed.\n");
     printf("*src and *new_src are not identical\n");
     return 0;
    }

    printf("Validation done. The string we ended up with is:\n%s\n",
      decompressed_data);

    return 0;
}
