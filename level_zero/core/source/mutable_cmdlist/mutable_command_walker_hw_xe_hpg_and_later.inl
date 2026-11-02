/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace L0::MCL {

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setNumberThreadsPerThreadGroup(uint32_t numThreadPerThreadGroup) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    constexpr uint32_t numberThreadsPerThreadGroupIndex = 5;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto &cpuBufferIdd = cpuBufferWalker->getInterfaceDescriptor();
    if (cpuBufferIdd.getNumberOfThreadsInGpgpuThreadGroup() == numThreadPerThreadGroup) {
        return;
    }
    cpuBufferIdd.setNumberOfThreadsInGpgpuThreadGroup(numThreadPerThreadGroup);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getInterfaceDescriptor().getRawData(numberThreadsPerThreadGroupIndex) = cpuBufferIdd.getRawData(numberThreadsPerThreadGroupIndex);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setNumberWorkGroups(MaxChannelsArray numberWorkGroups) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    cpuBufferWalker->setThreadGroupIdXDimension(numberWorkGroups[0]);
    cpuBufferWalker->setThreadGroupIdYDimension(numberWorkGroups[1]);
    cpuBufferWalker->setThreadGroupIdZDimension(numberWorkGroups[2]);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->setThreadGroupIdXDimension(numberWorkGroups[0]);
        walkerCmd->setThreadGroupIdYDimension(numberWorkGroups[1]);
        walkerCmd->setThreadGroupIdZDimension(numberWorkGroups[2]);
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setExecutionMask(uint32_t mask) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    if (cpuBufferWalker->getExecutionMask() == mask) {
        return;
    }
    cpuBufferWalker->setExecutionMask(mask);

    if (!this->stageCommitMode) {
        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->setExecutionMask(mask);
    }
}

template <typename GfxFamily>
void *MutableComputeWalkerHw<GfxFamily>::getInlineDataPointer() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);

    return walkerCmd->getInlineDataPointer();
}

template <typename GfxFamily>
void *MutableComputeWalkerHw<GfxFamily>::getHostMemoryInlineDataPointer() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);

    return cpuBufferWalker->getInlineDataPointer();
}

template <typename GfxFamily>
size_t MutableComputeWalkerHw<GfxFamily>::getInlineDataSize() const {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);

    return walkerCmd->getInlineDataSize();
}

template <typename GfxFamily>
size_t MutableComputeWalkerHw<GfxFamily>::getCommandSize() {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    return sizeof(WalkerType);
}

template <typename GfxFamily>
void *MutableComputeWalkerHw<GfxFamily>::createCommandBuffer() {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    return new WalkerType;
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::deleteCommandBuffer(void *input) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;
    if (input != nullptr) {
        delete (reinterpret_cast<WalkerType *>(input));
    }
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::setSlmSize(uint32_t slmSize) {
    using WalkerType = typename GfxFamily::DefaultWalkerType;

    auto cpuBufferWalker = reinterpret_cast<WalkerType *>(this->cpuBuffer);
    auto &cpuBufferIdd = cpuBufferWalker->getInterfaceDescriptor();

    if (cpuBufferIdd.getSharedLocalMemorySize() == slmSize) {
        return;
    }
    cpuBufferIdd.setSharedLocalMemorySize(slmSize);

    if (!this->stageCommitMode) {
        constexpr uint32_t slmSizeIddIndex = 5;

        auto walkerCmd = reinterpret_cast<WalkerType *>(this->walker);
        walkerCmd->getInterfaceDescriptor().getRawData(slmSizeIddIndex) = cpuBufferIdd.getRawData(slmSizeIddIndex);
    }
}

} // namespace L0::MCL
