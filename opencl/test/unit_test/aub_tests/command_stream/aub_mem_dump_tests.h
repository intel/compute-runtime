/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "aub_mapper_common.h"

namespace Os {
extern const char *fileSeparator;
}

extern std::string getAubFileName(const NEO::Device *pDevice, const std::string baseName);