#
# Copyright 2022, HENSOLDT Cyber
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: GPL-2.0-only
#

cmake_minimum_required(VERSION 3.7.2)

declare_platform(qemu-arm-virt KernelPlatformQEMUArmVirt PLAT_QEMU_ARM_VIRT KernelArchARM)

set(qemu_user_top 0xa0000000)

macro(setup_qemu_armv7 has_ve)
    cmake_parse_arguments(ARMV7_OPTIONS "ve" "" "")
    set(QEMU_ARCH "arm")
    set(KernelArchArmV7a ON)
    if(ARMV7_OPTIONS_ve)
        declare_seL4_arch(aarch32 arm_hyp)
        set(KernelArchArmV7ve ON)
        if(KernelSel4ArchArmHyp)
            set(qemu_user_top 0xe0000000)
        endif()
    else()
        declare_seL4_arch(aarch32)
    endif()
endmacro()

macro(setup_qemu_armv8)
    declare_seL4_arch(aarch64)
    set(QEMU_ARCH "aarch64")
    set(KernelArchArmV8a ON)
endmacro()

if(KernelPlatformQEMUArmVirt)

    if(NOT ARM_CPU)
        message(STATUS "ARM_CPU not set, defaulting to cortex-a53")
        set(ARM_CPU "cortex-a53")
    endif()

    if("${ARM_CPU}" STREQUAL "cortex-a7")
        setup_qemu_armv7()
        set(KernelArmCortexA7 ON)
    elseif("${ARM_CPU}" STREQUAL "cortex-a15")
        setup_qemu_armv7(ve)
        set(KernelArmCortexA15 ON)
    elseif("${ARM_CPU}" STREQUAL "cortex-a53")
        setup_qemu_armv8()
        set(KernelArmCortexA53 ON)
    elseif("${ARM_CPU}" STREQUAL "cortex-a57")
        setup_qemu_armv8()
        set(KernelArmCortexA57 ON)
    elseif("${ARM_CPU}" STREQUAL "cortex-a72")
        setup_qemu_armv8()
        set(KernelArmCortexA72 ON)
    else()
        message(FATAL_ERROR "Unsupported ARM_CPU: '${ARM_CPU}'")
    endif()

    config_set(KernelARMPlatform ARM_PLAT qemu-arm-virt)

    # If neither QEMU_DTS nor QEMU_DTB is set explicitly, the device tree is
    # extracted from QEMU. This keeps it nicely up to date with the the actual
    # QEMU versions that is used, and it's quite convenient for development.
    if(NOT DEFINED QEMU_DTS)

        set(QEMU_DTS "${CMAKE_BINARY_DIR}/qemu-arm-virt.dts")

        if(NOT DEFINED QEMU_DTB)

            message(STATUS "Extracting device tree from QEMU")
            set(QEMU_DTB "${CMAKE_BINARY_DIR}/qemu-arm-virt.dtb")

            # Use the system's QEMU if no custom QEMU is provided. Have a sanity
            # check about the version to ensure it can be used.
            if(NOT QEMU_BINARY)
                set(QEMU_BINARY "qemu-system-${QEMU_ARCH}")
                find_program(QEMU_BINARY ${QEMU_BINARY})
                # ARM virtual platform works since QEMU 3.1.0
                set(MIN_QEMU_VERSION "3.1.0")
                execute_process(
                    COMMAND ${QEMU_BINARY} -version
                    RESULT_VARIABLE error
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    OUTPUT_VARIABLE QEMU_STDOUT_MESSAGE
                )
                if(error)
                    message(FATAL_ERROR "Failed to determine QEMU version (${QEMU_BINARY})")
                endif()
                string(
                    REGEX
                        MATCH
                        "[0-9](\\.[0-9])+"
                        QEMU_VERSION
                        "${QEMU_STDOUT_MESSAGE}"
                )
                if("${QEMU_VERSION}" VERSION_LESS "${MIN_QEMU_VERSION}")
                    message(
                        FATAL_ERROR
                            "Error: need at least QEMU version ${MIN_QEMU_VERSION}, found '${QEMU_VERSION}'"
                    )
                endif()

            endif()

            if(NOT DEFINED QEMU_MACHINE)
                set(QEMU_MACHINE "virt")
                list(APPEND QEMU_MACHINE "secure=off")
                if(KernelArmHypervisorSupport OR KernelSel4ArchArmHyp)
                    list(APPEND QEMU_MACHINE "virtualization=on")
                endif()
                if(Kernel32)
                    list(APPEND QEMU_MACHINE "highmem=off")
                endif()



                # secure=[on|off] Security Extensions (TrustZone). The default is off.
                # virtualization=[on|off] Virtualization Extensions. The default is off.
                # mte=[on|off] Arm Memory Tagging Extensions. The default is off.
                # highmem=[on|off] Devices and RAM in physical address space above 32 bits. The default is on for machine types later than virt-2.12.
                # compact-highmem=[on|off] Compact layout for high memory regions. The default is on for machine types later than virt-7.2.
                # highmem-redists=[on|off] High memory region for GICv3 or GICv4 redistributor. The default is on. Setting this to off will limit the maximum number of CPUs when GICv3 or GICv4 is used.
                # highmem-ecam=[on|off] to High memory region for PCI ECAM. The default is on for machine types later than virt-3.0.
                # highmem-mmio=[on|off] to High memory region for PCI MMIO. The default is on.
                # gic-version=[2|3|4|host|max]:
                #   2: GICv2. Note that this limits the number of CPUs to 8.
                #   3: GICv3. This allows up to 512 CPUs.
                #   4: GICv4. Requires virtualization to be on; allows up to 317 CPUs.
                #   host: Use the same GIC version the host provides, when using KVM
                #   max: Use the best GIC version possible (same as host when using KVM; with TCG this is currently 3 if virtualization is off and 4 if virtualization is on, but this may change in future)
                list(APPEND QEMU_MACHINE "gic-version=2")
                # its=[on|off] ITS instantiation. The default is on for machine types later than virt-2.7.
                list(APPEND QEMU_MACHINE "its=off")
                # iommu=[none|smmuv3]: Set the IOMMU type
                # ras=[on|off] to enable/disable reporting host memory errors to a guest using ACPI and guest external abort exceptions. The default is off.
                list(APPEND QEMU_MACHINE "iommu=none")
                # dtb-randomness=[on|off] to pass random seeds via the guest DTB rng-seed and kaslr-seed nodes (in both “/chosen” and “/secure-chosen”) to use for features like the random number generator and address space randomisation. The default is on. You will want to disable it if your trusted boot chain will verify the DTB it is passed, since this option causes the DTB to be non-deterministic. It would be the responsibility of the firmware to come up with a seed and pass it on if it wants to.
                # dtb-kaslr-seed: A deprecated synonym for dtb-randomness.



                list(APPEND QEMU_MACHINE "dumpdtb=${QEMU_DTB}")

                # Lists are just strings with ";" as item separator, so we can
                # just replace them by ",". The cleaner way would be using the
                # CMake 3.12 feature: list(JOIN QEMU_MACHINE "," QEMU_MACHINE)
                string(
                    REPLACE
                        ";"
                        ","
                        QEMU_MACHINE
                        "${QEMU_MACHINE}"
                )

            endif()

            if(NOT DEFINED QEMU_MEMORY)
                # Having 1 GiB of memory as default is a legacy from the past
                # that is kept for compatibility. It should be increased to
                # 2 GiB, which seems a good trade-off nowadays. It's sufficient
                # for test/demo systems, but still something the host can
                # provide without running short on resources.
                set(QEMU_MEMORY "1024")
            endif()

            if(KernelMaxNumNodes)
                set(QEMU_SMP_OPTION "${KernelMaxNumNodes}")
            else()
                set(QEMU_SMP_OPTION "1")
            endif()

            # Run QEMU to get the device tree binary. Remember the command, so
            # it can be added to the DTS as reference. Every parameter must be
            # a separate string, as CMake will make it a dedicated argument
            # passed to QEMU then (e.g. "-machine virt" wont be recognized,
            # but "-machine", "vir" is).
            set(
                QEMU_CMD
                "${QEMU_BINARY}"
                "-machine"
                "${QEMU_MACHINE}"
                "-cpu"
                "${ARM_CPU}"
                "-smp"
                "${QEMU_SMP_OPTION}"
                "-m"
                "${QEMU_MEMORY}"
                "-nographic"
            )
            # When dumping the DTB to a file, QEMU prints a status message to
            # stderr. Capture it and print on stdout to avoid polluting stderr
            # unnecessarily.
            execute_process(
                COMMAND ${QEMU_CMD}
                RESULT_VARIABLE error
                ERROR_STRIP_TRAILING_WHITESPACE
                ERROR_VARIABLE QEMU_STDERR_MESSAGE
            )
            if(QEMU_STDERR_MESSAGE)
                message(STATUS "${QEMU_STDERR_MESSAGE}")
            endif()
            if(error)
                message(FATAL_ERROR "Failed to dump DTB using ${QEMU_BINARY}: ${error}")
            endif()

        endif()

        # At this point there is a DTB file, either it was passed or dumped from
        # QEMU. Create a DTS and store it in QEMU_DTS_DATA.
        execute_process(
            COMMAND
                dtc -q -I dtb -O dts "${QEMU_DTB}"
            OUTPUT_VARIABLE QEMU_DTS_DATA
            RESULT_VARIABLE error
        )
        if(error)
            message(FATAL_ERROR "Failed to create DTS from QEMU's DTB ${QEMU_DTB}: ${error}")
        endif()

        set(QEMU_DTS_INFO "converted from ${QEMU_DTB}")
        if(QEMU_CMD)
            # Lists are just strings with ";" as item separator, so we can
            # simply replace them with something else. The cleaner way is a
            # CMake 3.12 feature: list(JOIN QEMU_CMD "\n *    " QEMU_DTS_INFO)
            string(
                REPLACE
                    ";"
                    "\n *    "
                    QEMU_DTS_INFO
                    "DTS of QEMU v${QEMU_VERSION} for:;${QEMU_CMD}"
            )
        endif()
        file(
            WRITE
                "${QEMU_DTS}"
                "/*\n"
                " * ${QEMU_DTS_INFO}\n"
                " */\n"
                "\n"
                "${QEMU_DTS_DATA}"
        )

    endif()

    list(APPEND KernelDTSList "${QEMU_DTS}" "${CMAKE_CURRENT_LIST_DIR}/overlay-qemu-arm-virt.dts")

    if(KernelArmHypervisorSupport OR KernelSel4ArchArmHyp)
        list(APPEND KernelDTSList "${CMAKE_CURRENT_LIST_DIR}/overlay-reserve-vm-memory.dts")
    endif()

    declare_default_headers(
        TIMER_FREQUENCY 62500000
        MAX_IRQ 159
        NUM_PPI 32
        TIMER drivers/timer/arm_generic.h
        INTERRUPT_CONTROLLER arch/machine/gic_v2.h
        CLK_MAGIC 4611686019llu
        CLK_SHIFT 58u
        KERNEL_WCET 10u
    )

endif()

add_sources(
    DEP "KernelPlatformQEMUArmVirt"
    CFILES src/arch/arm/machine/gic_v2.c src/arch/arm/machine/l2c_nop.c
)

config_string(
    KernelUserTop USER_TOP "Set seL4_UserTop constant"
    DEFAULT ${qemu_user_top}
    UNQUOTE
    DEPENDS "KernelPlatformQEMUArmVirt;KernelSel4ArchAarch32"
)
