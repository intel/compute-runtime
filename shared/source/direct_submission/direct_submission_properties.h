/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "aubstream/engine_node.h"

#include <cstdint>
#include <initializer_list>
#include <utility>

namespace NEO {

struct DirectSubmissionProperties {
    bool engineSupported = false;
    bool submitOnInit = false;
    bool useNonDefault = false;
    bool useRootDevice = false;
    bool operator==(const DirectSubmissionProperties &) const = default;
};

struct DirectSubmissionPropertiesPerEngine {

    DirectSubmissionProperties data[aub_stream::NUM_ENGINES] = {};
    bool operator==(const DirectSubmissionPropertiesPerEngine &) const = default;
};

constexpr DirectSubmissionPropertiesPerEngine makeDirectSubmissionPropertiesPerEngine(
    std::initializer_list<std::pair<aub_stream::EngineType, DirectSubmissionProperties>> init) {
    DirectSubmissionPropertiesPerEngine out{};
    for (const auto &[engineType, directSubmissionProperties] : init) {
        out.data[static_cast<uint32_t>(engineType)] = directSubmissionProperties;
    }
    return out;
}

} // namespace NEO
