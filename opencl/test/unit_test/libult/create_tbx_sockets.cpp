/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/source/tbx/tbx_sockets_imp.h"
#include "opencl/test/unit_test/mocks/mock_tbx_sockets.h"

namespace NEO {
TbxSockets *TbxSockets::create() {
    if (testMode == TestMode::AubTestsWithTbx) {
        return new TbxSocketsImp;
    }
    return new MockTbxSockets;
}
} // namespace NEO
