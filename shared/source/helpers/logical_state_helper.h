/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
class LinearStream;

class LogicalStateHelper {
  public:
    template <typename Family>
    static LogicalStateHelper *create();

    virtual ~LogicalStateHelper() = default;

    virtual void writeStreamInline(LinearStream &linearStream, bool pipelinedState) = 0;
    virtual void mergePipelinedState(const LogicalStateHelper &inputLogicalStateHelper) {}

  protected:
    LogicalStateHelper() = default;
};

} // namespace NEO