/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ail_configuration_nvlp.inl"

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_NVL> enableAILNVL;

template class AILConfigurationHw<IGFX_NVL>;

} // namespace NEO
