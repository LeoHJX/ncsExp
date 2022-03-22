#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.13.1)


set(mcuboot_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf)
#if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay.conf")
#   list(APPEND mcuboot_OVERLAY_CONFIG
#      "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay.conf"
#      )
#endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripheral_cts_client)


FILE(GLOB app_sources src/*.c)
# NORDIC SDK APP START
target_sources(app PRIVATE ${app_sources})
# NORDIC SDK APP END

