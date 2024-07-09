/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {

template <>
void setSbaStatelessCompressionParams<Xe2HpgCoreFamily>(typename Xe2HpgCoreFamily::STATE_BASE_ADDRESS *stateBaseAddress, MemoryCompressionState memoryCompressionState) {
}

template struct StateBaseAddressHelper<Xe2HpgCoreFamily>;
} // namespace NEO
