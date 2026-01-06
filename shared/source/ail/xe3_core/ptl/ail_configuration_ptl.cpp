/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ail_configuration_ptl.inl"

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_PTL> enableAILPTL;

template class AILConfigurationHw<IGFX_PTL>;

} // namespace NEO
