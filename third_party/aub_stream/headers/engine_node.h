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
    ENGINE_CCCS,
    ENGINE_BCS1,
    ENGINE_BCS2,
    ENGINE_BCS3,
    ENGINE_BCS4,
    ENGINE_BCS5,
    ENGINE_BCS6,
    ENGINE_BCS7,
    ENGINE_BCS8,
    NUM_ENGINES
};

} // namespace aub_stream
