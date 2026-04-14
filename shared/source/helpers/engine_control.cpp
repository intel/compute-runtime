/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_control.h"

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

const aub_stream::EngineType &EngineControl::getEngineType() const {
    return osContext->getEngineType();
}

EngineUsage EngineControl::getEngineUsage() const {
    return osContext->getEngineUsage();
}

} // namespace NEO
