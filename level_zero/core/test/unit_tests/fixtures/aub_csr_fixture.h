/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace NEO {
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
} // namespace NEO

namespace L0 {
namespace ult {
struct AubCsrFixture : public ContextFixture {
    template <typename T>
    void SetUpT() {
        auto csrCreateFcn = &commandStreamReceiverFactory[IGFX_MAX_CORE + NEO::defaultHwInfo->platform.eRenderCoreFamily];
        variableBackup = std::make_unique<VariableBackup<CommandStreamReceiverCreateFunc>>(csrCreateFcn);
        *csrCreateFcn = UltAubCommandStreamReceiver<T>::create;
        ContextFixture::SetUp();
    }
    template <typename T>
    void TearDownT() {
        ContextFixture::TearDown();
    }

    void SetUp() {}
    void TearDown() {}
    std::unique_ptr<VariableBackup<CommandStreamReceiverCreateFunc>> variableBackup;
};

} // namespace ult
} // namespace L0
