/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct EngineInfo {
    EngineInfo() = default;
    virtual ~EngineInfo() = 0;
};

inline EngineInfo::~EngineInfo() {}

} // namespace NEO
