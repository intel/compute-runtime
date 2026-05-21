/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/front_end_controller.h"

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

FrontEndController::FrontEndController(Device *device) : device(device) {
}

FrontEndController::~FrontEndController() {
}

bool FrontEndController::initialize() {
    return false;
}

size_t FrontEndController::getFrontEndAllocationSize() const { return 0; }

} // namespace NEO
