/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "aubstream/engine_node.h"

#include <vector>

namespace NEO {

struct DirectSubmissionProperties {
    bool engineSupported = false;
    bool submitOnInit = false;
    bool useNonDefault = false;
    bool useRootDevice = false;
    bool useInternal = false;
    bool useLowPriority = false;
};

using EngineDirectSubmissionInitVec = std::vector<std::pair<aub_stream::EngineType, DirectSubmissionProperties>>;

struct DirectSubmissionProperyEngines {
    DirectSubmissionProperyEngines() = default;
    DirectSubmissionProperyEngines(const EngineDirectSubmissionInitVec &initData) {
        for (const auto &entry : initData) {
            data[entry.first] = entry.second;
        }
    }
    DirectSubmissionProperties data[aub_stream::NUM_ENGINES] = {};
};

} // namespace NEO
