/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/submissions_aggregator.h"

namespace NEO {
struct mockSubmissionsAggregator : public SubmissionAggregator {
    CommandBufferList &peekCommandBuffers() {
        return this->cmdBuffers;
    }
    uint32_t peekInspectionId() {
        return this->inspectionId;
    }
};
} // namespace NEO
