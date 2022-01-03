#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: GPL-2.0-only
#

''' generate a c header file from the device tree '''
import argparse
import builtins
import jinja2
from typing import Dict, List
import hardware
from hardware.fdt import FdtParser
from hardware.memory import Region
from hardware.utils.rule import HardwareYaml, KernelInterrupt


HEADER_TEMPLATE = '''/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/*
 * This file is autogenerated by <kernel>/tools/hardware/outputs/c_header.py.
 */

#pragma once

#define physBase {{ "0x{:x}".format(kernel_phy_base) }}

#ifndef __ASSEMBLER__

#include <config.h>
#include <mode/hardware.h>  /* for KDEV_BASE */
#include <linker.h>         /* for BOOT_RODATA */
#include <basic_types.h>    /* for p_region_t, kernel_frame_t (arch/types.h) */

/* INTERRUPTS */
{% for irq in kernel_irqs %}
/* {{ irq.desc }} */
{% if irq.has_enable() %}
{{ irq.get_enable_macro_str() }}
{% endif %}
{% if irq.has_sel() %}
{{ irq.get_sel_macro_str() }}
{% endif %}
#define {{ irq.label }} {{ irq.irq }}
{% if irq.has_sel() %}
#else
#define {{ irq.label }} {{ irq.false_irq }} /* dummy value */
{{ irq.get_sel_endif() }}
{% endif %}
{% if irq.has_enable() %}
{{ irq.get_enable_endif() }}
{% endif %}
{% endfor -%}

/* KERNEL DEVICES */
{% for (macro, addr) in kernel_dev_addr_macros %}
#define {{ macro }} (KDEV_BASE + {{ "0x{:x}".format(addr) }})
{% endfor %}

{% if len(kernel_regions) > 0 %}
static const kernel_frame_t BOOT_RODATA kernel_device_frames[] = {
    {% for group in kernel_regions %}
    {% if group.has_macro() %}
    {{ group.get_macro() }}
    {% endif %}
    /* {{ group.get_desc() }}
     * contains {{ ', '.join(group.labels.keys()) }}
     */
    {% for reg in group.regions %}
    {
        .paddr = {{ "0x{:x}".format(reg.base) }},
        .pptr = KDEV_BASE + {{ "0x{:x}".format(group.get_map_offset(reg)) }},
        {% if config.arch == 'arm' %}
        .armExecuteNever = true,
        {% endif %}
        .userAvailable = {{ str(group.user_ok).lower() }}
    },
    {% endfor %}
    {% if group.has_macro() %}
    {{ group.get_endif() }}
    {% endif %}
    {% endfor %}
};

/* Elements in kernel_device_frames may be enabled in specific configurations
 * only, but the ARRAY_SIZE() macro will automatically take care of this.
 * However, one corner case remains unsolved where all elements are disabled
 * and this becomes an empty array effectively. Then the C parser used in the
 * formal verification process will fail, because it follows the strict C rules
 * which do not allow empty arrays. Luckily, we have not met this case yet...
 */
#define NUM_KERNEL_DEVICE_FRAMES ARRAY_SIZE(kernel_device_frames)
{% else %}
/* The C parser used for formal verification process follows strict C rules,
 * which do not allow empty arrays. Thus this is defined as NULL.
 */
static const kernel_frame_t BOOT_RODATA *const kernel_device_frames = NULL;
#define NUM_KERNEL_DEVICE_FRAMES 0
{% endif %}

/* PHYSICAL MEMORY */
static const p_region_t BOOT_RODATA avail_p_regs[] = {
    {% for reg in physical_memory %}
    /* from {{ reg.owner.path }} */
    {
        .start = {{ "0x{:x}".format(reg.base) }},
        .end   = {{ "0x{:x}".format(reg.base + reg.size) }}
    },
    {% endfor %}
};

/* RESERVED REGIONS */
{% if len(reserved_regions) > 0 %}
static const p_region_t BOOT_RODATA reserved_p_regs[] = {
    {% for reg in reserved_regions %}
    /* from {{ reg.owner.path }} */
    {
        .start = {{ "0x{:x}".format(reg.base) }},
        .end   = {{ "0x{:x}".format(reg.base + reg.size) }}
    },
    {% endfor %}
};

#define NUM_RESERVED_PHYS_MEM_REGIONS ARRAY_SIZE(reserved_p_regs)
{% else %}
/* The C parser used for formal verification process follows strict C rules,
 * which do not allow empty arrays. Thus this is defined as NULL.
 */
static const p_region_t BOOT_RODATA *const reserved_p_regs = NULL;
#define NUM_RESERVED_PHYS_MEM_REGIONS 0
{% endif %}

#endif /* !__ASSEMBLER__ */

'''


def create_c_header_file(hw_yaml: HardwareYaml,
                         kernel_irqs: List[KernelInterrupt],
                         kernel_dev_addr_macros: Dict[str, int],
                         kernel_regions: List[Region], kernel_phy_base: int,
                         physical_memory: List[Region],
                         reserved_regions: List[Region],
                         outputStream):
    jinja_env = jinja2.Environment(loader=jinja2.BaseLoader, trim_blocks=True,
                                   lstrip_blocks=True)

    template = jinja_env.from_string(HEADER_TEMPLATE)
    template_args = dict(
        builtins.__dict__,
        **{
            'config': hw_yaml.config,
            'kernel_irqs': kernel_irqs,
            'kernel_dev_addr_macros': kernel_dev_addr_macros,
            'kernel_regions': kernel_regions,
            'kernel_phy_base': kernel_phy_base,
            'physical_memory': physical_memory,
            'reserved_regions': reserved_regions})
    data = template.render(template_args)

    with outputStream:
        outputStream.write(data)


def run(tree: FdtParser, hw_yaml: HardwareYaml, args: argparse.Namespace):
    if not args.header_out:
        raise ValueError('You need to specify a header-out to use c header output')

    # We only care about the available physical memory. The device memory
    # regions are not relevant here.
    physical_memory, reserved_regions, _ = \
        hardware.utils.memory.get_phys_mem_regions(tree, hw_yaml)

    # By convention, the first region holds the kernel image. But there is no
    # strict requirement that it must be in the first region, any region would
    # do. An exception is thrown if the region is too small to satisfy the
    # kernel's alignment requirement. Currently there is no solution for this,
    # so this must be solved by the first platform that runs into this problem.
    # During the boot process, the kernel will mark its own memory as reserved,
    # so there is no need to split the region here in a part before the kernel
    # and a part that has the kernel in the beginning.
    # Unfortunately, we do not know the kernel size here, so there is no
    # guarantee the kernel really fits into that region. For now, we leave this
    # issue to be resolved by the boot flow. Loading the ELF loader will fail
    # anyway if there is not enough space.
    reg = physical_memory[0]
    kernel_phys_align = hw_yaml.config.get_kernel_phys_align()
    kernel_phy_base = reg.base if kernel_phys_align == 0 \
        else reg.align_base(kernel_phys_align).base

    # Build a set or irqs and list of KernelRegionGroups, where each element
    # represents a single contiguous region of memory that is associated with a
    # device.
    kernel_irq_dict = {}  # dict of 'label:irq_obj'
    kernel_regions = []  # list of Regions.
    for dev in tree.get_kernel_devices():
        dev_rule = hw_yaml.get_rule(dev)

        if len(dev_rule.interrupts.items()) > 0:
            for irq in dev_rule.get_interrupts(tree, dev):
                # Add the interrupt if it does not exists or overwrite an
                # existing entry if the priority for this device is higher
                if (not irq.label in kernel_irq_dict) or \
                   (irq.prio > kernel_irq_dict[irq.label].prio):
                    kernel_irq_dict[irq.label] = irq

        for reg in dev_rule.get_regions(dev):
            existing_reg = next((r for r in kernel_regions if r == reg), None)
            if existing_reg:
                existing_reg.take_labels(reg)
            else:
                kernel_regions.append(reg)

    # Build a dict of 'label: offset' entries, where label is the name given to
    # the kernel for that address (e.g. SERIAL_PPTR) and offset is the offset
    # from KDEV_BASE at which it's mapped.
    kernel_dev_addr_macros = {}
    kernel_offset = 0
    for group in kernel_regions:
        kernel_offset = group.set_kernel_offset(kernel_offset)
        for (offset, label) in group.get_labelled_addresses().items():
            if label in kernel_dev_addr_macros:
                raise ValueError(
                    '"{} = 0x{:x}" already exists, cannot change to 0x{:x}'.format(
                        label, kernel_dev_addr_macros[label], offset))
            kernel_dev_addr_macros[label] = offset

    create_c_header_file(
        hw_yaml,
        sorted(kernel_irq_dict.values(), key=lambda irq: irq.label),
        sorted(kernel_dev_addr_macros.items(), key=lambda tupel: tupel[1]),
        kernel_regions,
        kernel_phy_base,
        physical_memory,
        reserved_regions,
        args.header_out)


def add_args(parser):
    parser.add_argument('--header-out', help='output file for c header',
                        type=argparse.FileType('w'))
