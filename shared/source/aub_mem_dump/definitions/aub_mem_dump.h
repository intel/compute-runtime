/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

#ifndef BIT
#define BIT(x) (((uint64_t)1) << (x))
#endif

#include "shared/source/aub_mem_dump/aub_data.h"

namespace NEO {
class AubHelper;
}

namespace AubMemDump {
#include "shared/source/aub_mem_dump/aub_mem_dump_base.inl"
} // namespace AubMemDump
