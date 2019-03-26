/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"

#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/hw_info.h"

namespace NEO {
const char *folderAUB = "aub_out";

uint32_t initialHardwareTag = static_cast<uint32_t>(0xFFFFFF00);

static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

size_t numPlatformDevices = arrayCount(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
