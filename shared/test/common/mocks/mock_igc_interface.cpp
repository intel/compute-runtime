/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_igc_interface.inl"

namespace IGC {
#include "cif/macros/enable.h"
#define DEFINE_GET_SET(INTERFACE, VERSION, NAME, TYPE)                                      \
    TYPE CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Get##NAME() const { return (TYPE)0; } \
    void CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Set##NAME(TYPE v) {}

DEFINE_GET_SET(IgcFeaturesAndWorkarounds, 4, FtrEfficient64BitAddressing, bool);

#undef DEFINE_GET_SET
#include "cif/macros/disable.h"
} // namespace IGC
