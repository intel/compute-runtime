/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_plus.inl"
#include "shared/source/xe_hp_core/hw_cmds.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_xehp_plus.inl"

namespace NEO {

template <>
void HardwareCommandsHelper<XeHpFamily>::setGrfInfo(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const Kernel &kernel,
                                                    const size_t &sizeCrossThreadData, const size_t &sizePerThreadData) {
}

template struct HardwareCommandsHelper<XeHpFamily>;
} // namespace NEO
