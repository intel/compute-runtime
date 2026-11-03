/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
size_t BlitCommandsHelper<Family>::getNumberOfBlitsForByteFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    return NEO::BlitCommandsHelper<Family>::getNumberOfBlitsForFill(copySize, patternSize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
}

template <>
BlitCommandsResult BlitCommandsHelper<Family>::dispatchBlitMemoryByteFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    return NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryColorFill(blitProperties, linearStream, rootDeviceEnvironment);
}

template <>
void BlitCommandsHelper<Family>::appendBlitMemSetCompressionFormat(void *blitCmd, NEO::GraphicsAllocation *dstAlloc, uint32_t compressionFormat) {}
