/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

namespace NEO {

bool Device::isBlitSplitEnabled() {
    return true;
}

bool Device::isInitDeviceWithFirstSubmissionEnabled(CommandStreamReceiverType csrType) {
    return csrType == CommandStreamReceiverType::hardware;
}

} // namespace NEO
