/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <array>
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
    static const size_t numAllEngineInstances = 5;
    static const size_t numGpgpuEngineInstances = 2;

    constexpr EngineInstanceT(EngineType type) : type(type), id(0) {}
    constexpr EngineInstanceT(EngineType type, int id) : type(type), id(id) {}

    EngineType type;
    int id;
};

static constexpr std::array<EngineInstanceT, EngineInstanceT::numAllEngineInstances> allEngineInstances = {{
    {ENGINE_RCS, 0},
    {ENGINE_RCS, 1},
    {ENGINE_BCS},
    {ENGINE_VCS},
    {ENGINE_VECS},
}};

static constexpr std::array<EngineInstanceT, EngineInstanceT::numGpgpuEngineInstances> gpgpuEngineInstances = {{
    {ENGINE_RCS, 0},
    {ENGINE_RCS, 1},
}};
} // namespace OCLRT
