/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_compilers.h"

#include "cif/macros/enable.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

namespace IGC {

#define DEFINE_GET_SET(INTERFACE, VERSION, NAME, TYPE)                                      \
    TYPE CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Get##NAME() const { return (TYPE)0; } \
    void CIF_GET_INTERFACE_CLASS(INTERFACE, VERSION)::Set##NAME(TYPE v) {}

DEFINE_GET_SET(GTSystemInfo, 2, DualSubSliceCount, uint32_t);

#undef DEFINE_GET_SET
} // namespace IGC
