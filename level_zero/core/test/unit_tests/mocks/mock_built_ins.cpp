/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"

#include "opencl/source/helpers/built_ins_helper.h"

namespace L0 {
namespace ult {

const NEO::SipKernel &MockBuiltins::getSipKernel(NEO::SipKernelType type, NEO::Device &device) {
    return NEO::initSipKernel(type, device);
}

} // namespace ult
} // namespace L0
