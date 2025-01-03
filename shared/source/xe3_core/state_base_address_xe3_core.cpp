/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

using GfxFamily = Xe3CoreFamily;

template <>
void setSbaStatelessCompressionParams<GfxFamily>(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress, MemoryCompressionState memoryCompressionState) {
}

template struct StateBaseAddressHelper<GfxFamily>;
} // namespace NEO
