/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
struct Device;

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
