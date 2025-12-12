/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
template <>
void NEO::HardwareCommandsHelper<NEO::FamilyType>::programInlineData<NEO::FamilyType::COMPUTE_WALKER_2>(
    Kernel &kernel, FamilyType::COMPUTE_WALKER_2 *walkerCmd, uint64_t indirectDataAddress, uint64_t scratchAddress) {

    const auto &kernelDescriptor = kernel.getKernelInfo().kernelDescriptor;
    auto indirectDataPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress;
    auto scratchPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress;
    uint8_t *inlineDataPtr = reinterpret_cast<uint8_t *>(walkerCmd->getInlineDataPointer());
    uint32_t inlineDataSize = walkerCmd->getInlineDataSize();

    if (isDefined(indirectDataPointerAddress.pointerSize) && isValidOffset(indirectDataPointerAddress.offset)) {
        uint32_t maxBytesToCopy = std::max(0, static_cast<int32_t>(inlineDataSize - indirectDataPointerAddress.offset));
        memcpy_s(inlineDataPtr + indirectDataPointerAddress.offset, maxBytesToCopy, &indirectDataAddress, indirectDataPointerAddress.pointerSize);
    }
    if (isDefined(scratchPointerAddress.pointerSize) && isValidOffset(scratchPointerAddress.offset)) {
        uint32_t maxBytesToCopy = std::max(0, static_cast<int32_t>(inlineDataSize - scratchPointerAddress.offset));
        memcpy_s(inlineDataPtr + scratchPointerAddress.offset, maxBytesToCopy, &scratchAddress, scratchPointerAddress.pointerSize);
    }
}
