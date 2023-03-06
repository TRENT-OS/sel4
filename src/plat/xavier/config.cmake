#
# Copyright 2022, HENSOLDT Cyber GmbH
#
# SPDX-License-Identifier: GPL-2.0-only
#

cmake_minimum_required(VERSION 3.7.2)

declare_platform(xavier KernelPlatformXavier PLAT_XAVIER KernelSel4ArchAarch64)

# disable platform specific settings by default in cache, will be enabled below
# if active
foreach(
    var
    IN
    ITEMS
    KernelPlatformJetsonXavierNXDevKit
    KernelPlatformAetinaAN110XNX
)
    unset(${var} CACHE)
    set(${var} OFF)
endforeach()

if(KernelPlatformXavier)

    check_platform_and_fallback_to_default(KernelARMPlatform "jetson-xavier-nx-dev-kit")

    if(KernelARMPlatform STREQUAL "jetson-xavier-nx-dev-kit")
        config_set(KernelPlatformJetsonXavierNXDevKit PLAT_JETSON_XAVIER_NX_DEV_KIT ON)
    elseif(KernelARMPlatform STREQUAL "aetina-an110-xnx")
        config_set(KernelPlatformAetinaAN110XNX PLAT_AETINA_AN110_XNX ON)
    else()
        message(FATAL_ERROR "Which Xavier platform not specified")
    endif()

    config_set(KernelARMPlatform ARM_PLAT ${KernelARMPlatform})
    declare_seL4_arch(aarch64)
    set(KernelNvidiaCarmel ON)
    set(KernelArchArmV8a ON)
    set(KernelArmSMMU OFF)
    set(KernelAArch64SErrorIgnore ON)
    set(KernelArmMach "nvidia" CACHE INTERNAL "")
    list(APPEND KernelDTSList "tools/dts/${KernelARMPlatform}.dts")
    list(APPEND KernelDTSList "src/plat/xavier/overlay-${KernelARMPlatform}.dts")
    declare_default_headers(
        TIMER_FREQUENCY 31250000
        MAX_IRQ 416
        INTERRUPT_CONTROLLER arch/machine/gic_v2.h
        NUM_PPI 32
        TIMER drivers/timer/arm_generic.h
        CLK_SHIFT 57u
        CLK_MAGIC 4611686019u
        KERNEL_WCET 10u SMMU drivers/smmu/smmuv2.h
        MAX_SID 128
        MAX_CB 64
    )
endif()

add_sources(
    DEP "KernelPlatformXavier"
    CFILES src/arch/arm/machine/gic_v2.c src/arch/arm/machine/l2c_nop.c
)