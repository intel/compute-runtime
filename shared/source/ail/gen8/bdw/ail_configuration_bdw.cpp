/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_BROADWELL> enableAILBDW;

template class AILConfigurationHw<IGFX_BROADWELL>;

} // namespace NEO
