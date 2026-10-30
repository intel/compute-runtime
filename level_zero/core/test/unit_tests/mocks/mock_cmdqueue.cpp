/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {
struct Device;

namespace ult {

WhiteBox<::L0::CommandQueue>::WhiteBox(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
    : ::L0::CommandQueue(device, csr, desc) {}

WhiteBox<::L0::CommandQueue>::~WhiteBox() {}

Mock<CommandQueue>::Mock(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
    : WhiteBox<::L0::CommandQueue>(device, csr, desc) {
    this->device = device;
    if (device && csr && csr->isInitialized() &&
        L0GfxCoreHelper::getHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()) == NEO::HeapAddressModel::globalStateless) {
        csr->createGlobalStatelessHeap();
    }
}

Mock<CommandQueue>::~Mock() {
}

} // namespace ult
} // namespace L0
