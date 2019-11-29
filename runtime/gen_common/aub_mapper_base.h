/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/completion_stamp.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"

#include <vector>

namespace NEO {

template <typename GfxFamily>
struct AUBFamilyMapper {
};

using MMIOPair = std::pair<uint32_t, uint32_t>;
using MMIOList = std::vector<MMIOPair>;

} // namespace NEO
