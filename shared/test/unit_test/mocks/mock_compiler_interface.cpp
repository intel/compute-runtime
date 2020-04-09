/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_compiler_interface.h"

#include "opencl/test/unit_test/mocks/mock_sip.h"

namespace NEO {
std::vector<char> MockCompilerInterface::getDummyGenBinary() {
    return MockSipKernel::getDummyGenBinary();
}
} // namespace NEO
