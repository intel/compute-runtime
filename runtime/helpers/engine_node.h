/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
namespace OCLRT {

enum EngineType : uint32_t {
    ENGINE_RCS = 0,
    ENGINE_BCS,
    ENGINE_VCS,
    ENGINE_VECS,
    NUM_ENGINES
};

struct EngineInstanceT {
    constexpr EngineInstanceT(EngineType type) : type(type), id(0) {}
    constexpr EngineInstanceT(EngineType type, int id) : type(type), id(id) {}

    EngineType type;
    int id;
};

static constexpr EngineInstanceT allEngineInstances[] = {
    {ENGINE_RCS},
    {ENGINE_BCS},
    {ENGINE_VCS},
    {ENGINE_VECS},
};

static constexpr EngineInstanceT gpgpuEngineInstances[] = {
    {ENGINE_RCS},
};
} // namespace OCLRT
