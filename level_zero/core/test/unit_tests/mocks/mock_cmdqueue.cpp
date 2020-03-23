/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_cmdqueue.h"

#include "shared/source/device/device.h"

namespace L0 {
namespace ult {

WhiteBox<::L0::CommandQueue>::WhiteBox(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
    : ::L0::CommandQueueImp(device, csr, desc) {}

WhiteBox<::L0::CommandQueue>::~WhiteBox() {}

Mock<CommandQueue>::Mock(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
    : WhiteBox<::L0::CommandQueue>(device, csr, desc) {
    this->device = device;
}

Mock<CommandQueue>::~Mock() {
}

} // namespace ult
} // namespace L0
