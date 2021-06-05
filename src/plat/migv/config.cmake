#
# Copyright 2020-2021, HENSOLDT Cyber GmbH
#
# SPDX-License-Identifier: BSD-2-Clause and GPL-2.0-or-later
#

cmake_minimum_required(VERSION 3.7.2)

declare_platform(migv KernelPlatformMiGV PLAT_MIGV KernelSel4ArchRiscV64)

if(KernelPlatformMiGV)
    declare_seL4_arch(riscv64)
    config_set(KernelRiscVPlatform RISCV_PLAT "migv")
    config_set(KernelPlatformFirstHartID FIRST_HART_ID 0)
    config_set(KernelOpenSBIPlatform OPENSBI_PLATFORM "generic")

    # ROM Kernel
    config_set(KernelDontRecycleBootMem DONT_RECYCLE_BOOT_MEM ON)

    list(
        APPEND
        KernelDTSList
        "tools/dts/migv.dts"
        "${CMAKE_CURRENT_LIST_DIR}/overlay-migv.dts"
    )
    declare_default_headers(
        TIMER_FREQUENCY       32768 # 32 KHz RTC
        PLIC_MAX_NUM_INT      24  # MiG-V 1.0 has 24 interrupts (1 - 24)
        INTERRUPT_CONTROLLER  "drivers/irq/riscv-hcsc1.h"
    )
else()
    unset(KernelPlatformFirstHartID CACHE)
endif()
