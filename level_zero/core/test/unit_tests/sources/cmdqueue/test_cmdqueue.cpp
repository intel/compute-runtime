/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using CommandQueueCreate = Test<DeviceFixture>;

TEST_F(CommandQueueCreate, whenCreatingCommandQueueThenItIsInitialized) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device.get(),
                                                          csr.get(),
                                                          &desc);
    ASSERT_NE(nullptr, commandQueue);

    L0::CommandQueueImp *commandQueueImp = reinterpret_cast<L0::CommandQueueImp *>(commandQueue);

    EXPECT_EQ(csr.get(), commandQueueImp->getCsr());
    EXPECT_EQ(device.get(), commandQueueImp->getDevice());
    EXPECT_EQ(0u, commandQueueImp->getTaskCount());

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0