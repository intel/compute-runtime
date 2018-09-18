/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/hw_info.h"

namespace OCLRT {
const char *folderAUB = "aub_out";

uint32_t initialHardwareTag = static_cast<uint32_t>(0xFFFFFF00);

static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

size_t numPlatformDevices = arrayCount(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace OCLRT
