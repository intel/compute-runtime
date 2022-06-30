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
    static LogicalStateHelper *create(bool pipelinedState);

    virtual ~LogicalStateHelper() = default;

    virtual void writeStreamInline(LinearStream &linearStream) = 0;

  protected:
    LogicalStateHelper(bool pipelinedState){};
    LogicalStateHelper() = delete;
};

} // namespace NEO