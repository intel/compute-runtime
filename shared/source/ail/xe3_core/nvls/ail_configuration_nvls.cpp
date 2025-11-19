/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<nvlsProductEnumValue> enableAILNVLS;

template class AILConfigurationHw<nvlsProductEnumValue>;

} // namespace NEO
