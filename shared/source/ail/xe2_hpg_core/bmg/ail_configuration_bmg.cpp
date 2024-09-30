/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ail_configuration_bmg.inl"

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_BMG> enableAILBMG;

template <>
bool AILConfigurationHw<IGFX_BMG>::is256BPrefetchDisableRequired() {
    auto iterator = applicationsOverfetchDisabled.find(processName);
    return iterator != applicationsOverfetchDisabled.end();
}

template class AILConfigurationHw<IGFX_BMG>;
} // namespace NEO
