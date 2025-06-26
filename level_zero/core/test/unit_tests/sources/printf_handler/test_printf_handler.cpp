/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

TEST(PrintfHandler, whenPrintfBufferIscreatedThenCorrectAllocationTypeIsUsed) {
    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(neoDevice);

    auto allocation = PrintfHandler::createPrintfBuffer(&l0Device);
    EXPECT_EQ(NEO::AllocationType::printfSurface, allocation->getAllocationType());
    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

} // namespace ult
} // namespace L0
