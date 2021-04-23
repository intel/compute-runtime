/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace aub_stream {

enum EngineType : uint32_t {
    ENGINE_RCS = 0,
    ENGINE_BCS,
    ENGINE_VCS,
    ENGINE_VECS,
    ENGINE_CCS,
    ENGINE_CCS1,
    ENGINE_CCS2,
    ENGINE_CCS3,
    NUM_ENGINES
};

} // namespace aub_stream
