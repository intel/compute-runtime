/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/completion_stamp.h"

#include "aub_mem_dump.h"

#include <vector>

namespace NEO {

template <typename GfxFamily>
struct AUBFamilyMapper {
};

using MMIOPair = std::pair<uint32_t, uint32_t>;
using MMIOList = std::vector<MMIOPair>;

} // namespace NEO
