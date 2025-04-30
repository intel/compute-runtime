/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace NEO {
extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
} // namespace NEO

namespace L0 {
namespace ult {
struct AubCsrFixture : public DeviceFixture {
    template <typename T>
    void setUpT() {
        auto csrCreateFcn = &commandStreamReceiverFactory[IGFX_MAX_CORE + NEO::defaultHwInfo->platform.eRenderCoreFamily];
        variableBackup = std::make_unique<VariableBackup<CommandStreamReceiverCreateFunc>>(csrCreateFcn);
        *csrCreateFcn = UltAubCommandStreamReceiver<T>::create;
        DeviceFixture::setUp();
    }
    template <typename T>
    void tearDownT() {
        DeviceFixture::tearDown();
    }

    void setUp() {}
    void tearDown() {}
    std::unique_ptr<VariableBackup<CommandStreamReceiverCreateFunc>> variableBackup;
};

} // namespace ult
} // namespace L0
