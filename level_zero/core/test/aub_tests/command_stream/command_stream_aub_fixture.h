/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"

#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"

#include <memory>
#include <vector>

namespace NEO {
enum class AllocationType;
class GraphicsAllocation;
struct DispatchFlags;
} // namespace NEO

namespace L0 {
namespace ult {

struct CommandStreamAubFixture : public AUBFixtureL0 {
    void setUp();
    void tearDown();

    NEO::GraphicsAllocation *allocate(size_t size, NEO::AllocationType allocationType);
    NEO::GraphicsAllocation *createBufferWithData(const void *hostPtr, size_t size);
    void flushStream();
    void flushStream(size_t streamStartOffset, NEO::DispatchFlags &dispatchFlags);

    std::unique_ptr<NEO::LinearStream> taskStream;
    std::vector<NEO::GraphicsAllocation *> allocations;
};

} // namespace ult
} // namespace L0
