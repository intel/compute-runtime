/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/helpers/ult_hw_config.h"
namespace NEO {

bool Device::isBlitSplitEnabled() {
    return ultHwConfig.useBlitSplit;
}

bool Device::isInitDeviceWithFirstSubmissionEnabled(CommandStreamReceiverType csrType) {
    return ultHwConfig.useFirstSubmissionInitDevice;
}

} // namespace NEO
