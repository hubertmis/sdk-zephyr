# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/ack_generator/nrf_802154_ack_data.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/ack_generator/nrf_802154_ack_generator.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/ack_generator/nrf_802154_enh_ack_generator.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/ack_generator/nrf_802154_imm_ack_generator.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/nrf_802154_frame_parser.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/nrf_802154_ie_writer.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/mac_features/nrf_802154_security_pib_ram.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/nrf_802154_encrypt_dummy.c
    $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src/nrf_802154_pib.c
    )

target_include_directories(app PRIVATE
   $ENV{ZEPHYR_BASE}/../nrf-802154/driver/include
   $ENV{ZEPHYR_BASE}/../nrf-802154/driver/src
   $ENV{ZEPHYR_BASE}/../nrf-802154/sl/include
   )
