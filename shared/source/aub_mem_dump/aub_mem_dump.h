/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_header.h"

namespace AubMemDump {

#ifndef BIT
#define BIT(x) (((uint64_t)1) << (x))
#endif

typedef CmdServicesMemTraceVersion::SteppingValues SteppingValues;
typedef CmdServicesMemTraceMemoryWrite::DataTypeHintValues DataTypeHintValues;

struct LrcaHelper {
    static void setContextSaveRestoreFlags(uint32_t &value);
};

} // namespace AubMemDump
