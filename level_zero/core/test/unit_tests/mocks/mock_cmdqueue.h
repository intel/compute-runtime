/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::CommandQueue> : public ::L0::CommandQueueImp {
    using BaseClass = ::L0::CommandQueueImp;
    using BaseClass::buffers;
    using BaseClass::commandStream;
    using BaseClass::csr;
    using BaseClass::device;
    using BaseClass::printfFunctionContainer;
    using BaseClass::synchronizeByPollingForTaskCount;

    WhiteBox(Device *device, NEO::CommandStreamReceiver *csr,
             const ze_command_queue_desc_t *desc);
    ~WhiteBox() override;
};

using CommandQueue = WhiteBox<::L0::CommandQueue>;
static ze_command_queue_desc_t default_cmd_queue_desc = {};
template <>
struct Mock<CommandQueue> : public CommandQueue {
    Mock(L0::Device *device = nullptr, NEO::CommandStreamReceiver *csr = nullptr, const ze_command_queue_desc_t *desc = &default_cmd_queue_desc);
    ~Mock() override;

    MOCK_METHOD2(createFence, ze_result_t(const ze_fence_desc_t *desc, ze_fence_handle_t *phFence));
    MOCK_METHOD0(destroy, ze_result_t());
    MOCK_METHOD4(executeCommandLists,
                 ze_result_t(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                             ze_fence_handle_t hFence, bool performMigration));
    MOCK_METHOD3(executeCommands, ze_result_t(uint32_t numCommands,
                                              void *phCommands,
                                              ze_fence_handle_t hFence));
    MOCK_METHOD1(synchronize, ze_result_t(uint32_t timeout));

    MOCK_METHOD2(dispatchTaskCountWrite, void(NEO::LinearStream &commandStream, bool flushDataCache));
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    MockCommandQueueHw(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : L0::CommandQueueHw<gfxCoreFamily>(device, csr, desc) {
    }
    ze_result_t synchronize(uint32_t timeout) override {
        synchronizedCalled++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t synchronizedCalled = 0;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
