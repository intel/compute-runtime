/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/tbx/tbx_sockets_imp.h"
#include "shared/test/common/mocks/mock_tbx_sockets.h"
#include "shared/test/common/tests_configuration.h"

namespace NEO {
TbxSockets *TbxSockets::create() {
    if (testMode == TestMode::aubTestsWithTbx) {
        return new TbxSocketsImp;
    }
    return new MockTbxSockets;
}
} // namespace NEO
