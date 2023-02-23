#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: GPL-2.0-only
#

cmake_minimum_required(VERSION 3.7.2)

declare_platform(tx1 KernelPlatformTx1 PLAT_TX1 KernelSel4ArchAarch64)

# disable platform specific settings by default in cache, will be enabled below
# if active
foreach(
    var
    IN
    ITEMS
    KernelPlatformJetsonTX1DevKit
    KernelPlatformJetsonNano2GBDevKit
)
    unset(${var} CACHE)
    set(${var} OFF)
endforeach()

if(KernelPlatformTx1)

    check_platform_and_fallback_to_default(KernelARMPlatform "jetson-tx1-dev-kit")

    if(KernelARMPlatform STREQUAL "jetson-tx1-dev-kit")
        config_set(KernelPlatformJetsonTX1DevKit PLAT_JETSON_TX1_DEV_KIT ON)
    elseif(KernelARMPlatform STREQUAL "jetson-nano-2gb-dev-kit")
        config_set(KernelPlatformJetsonNano2GBDevKit PLAT_JETSON_NANO_2GB_DEV_KIT ON)
    else()
        message(FATAL_ERROR "Which TX1 platform not specified")
    endif()

    config_set(KernelARMPlatform ARM_PLAT ${KernelARMPlatform})
    declare_seL4_arch(aarch64)
    set(KernelArmCortexA57 ON)
    set(KernelArchArmV8a ON)
    # config_set(KernelARMPlatform ARM_PLAT tx1)
    set(KernelArmMach "nvidia" CACHE INTERNAL "")
    list(APPEND KernelDTSList "tools/dts/${KernelARMPlatform}.dts")
    list(APPEND KernelDTSList "src/plat/tx1/overlay-${KernelARMPlatform}.dts")

    if(KernelARMPlatform STREQUAL "jetson-tx1-dev-kit")
        declare_default_headers(
            TIMER_FREQUENCY 12000000
            MAX_IRQ 224
            INTERRUPT_CONTROLLER arch/machine/gic_v2.h
            NUM_PPI 32
            TIMER drivers/timer/arm_generic.h
            CLK_MAGIC 2863311531llu
            CLK_SHIFT 35u
            KERNEL_WCET 10u
        )
    elseif(KernelARMPlatform STREQUAL "jetson-nano-2gb-dev-kit")
        declare_default_headers(
            TIMER_FREQUENCY 19200000
            MAX_IRQ 224
            INTERRUPT_CONTROLLER arch/machine/gic_v2.h
            NUM_PPI 32
            TIMER drivers/timer/arm_generic.h
            CLK_MAGIC 2863311531llu
            CLK_SHIFT 35u
            KERNEL_WCET 10u
        )
    endif()
endif()

add_sources(
    DEP "KernelPlatformTx1"
    CFILES src/arch/arm/machine/gic_v2.c src/arch/arm/machine/l2c_nop.c
)
