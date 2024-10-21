/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace NEO {
struct RootDeviceEnvironment;

struct EncodeDummyBlitWaArgs {
    bool isWaRequired = false;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

struct MiFlushArgs {
    bool timeStampOperation = false;
    bool commandWithPostSync = false;
    bool notifyEnable = false;
    bool tlbFlush = false;

    EncodeDummyBlitWaArgs &waArgs;
    MiFlushArgs(EncodeDummyBlitWaArgs &args) : waArgs(args) {}
};

enum class RequiredPartitionDim : uint32_t {
    none = 0,
    x,
    y,
    z
};

enum class RequiredDispatchWalkOrder : uint32_t {
    none = 0,
    x,
    y,
    additional
};

static constexpr uint32_t additionalKernelLaunchSizeParamNotSet = 0;

namespace EncodeParamsApiMappings {
static constexpr std::array<NEO::RequiredPartitionDim, 3> partitionDim = {{RequiredPartitionDim::x, NEO::RequiredPartitionDim::y, NEO::RequiredPartitionDim::z}};
static constexpr std::array<NEO::RequiredDispatchWalkOrder, 3> walkOrder = {{NEO::RequiredDispatchWalkOrder::x, NEO::RequiredDispatchWalkOrder::y, NEO::RequiredDispatchWalkOrder::additional}};
} // namespace EncodeParamsApiMappings

} // namespace NEO
