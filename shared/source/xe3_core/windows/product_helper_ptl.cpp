/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_xe2_and_later.inl"
#include "shared/source/xe3_core/hw_cmds_ptl.h"
#include "shared/source/xe3_core/hw_info_ptl.h"

constexpr static auto gfxProduct = IGFX_PTL;

#include "shared/source/xe3_core/os_agnostic_product_helper_xe3_core.inl"
#include "shared/source/xe3_core/ptl/os_agnostic_product_helper_ptl.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const {
    const bool enabled = !isOOQ && queueTaskCount == queueCsr.peekTaskCount() && !queueCsr.directSubmissionRelaxedOrderingEnabled();
    if (debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return debugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template class ProductHelperHw<gfxProduct>;

} // namespace NEO
