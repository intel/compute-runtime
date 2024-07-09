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

template class AILConfigurationHw<IGFX_BMG>;
} // namespace NEO
