/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/graphics_allocation.h"

namespace NEO {

void GraphicsAllocation::setAubWritable(bool writable, uint32_t banks) { aubInfo.aubWritable = writable; }
bool GraphicsAllocation::isAubWritable(uint32_t banks) const { return (aubInfo.aubWritable != 0); }
void GraphicsAllocation::setTbxWritable(bool writable, uint32_t banks) { aubInfo.tbxWritable = writable; }
bool GraphicsAllocation::isTbxWritable(uint32_t banks) const { return (aubInfo.tbxWritable != 0); }

} // namespace NEO
