/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ail_configuration_nvls.inl"

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_NVL_XE3G> enableAILNVLS;

template class AILConfigurationHw<IGFX_NVL_XE3G>;

} // namespace NEO
