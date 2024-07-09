/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ail_configuration_lnl.inl"

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_LUNARLAKE> enableAILLNL;

template class AILConfigurationHw<IGFX_LUNARLAKE>;
} // namespace NEO
