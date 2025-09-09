/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/hw_timestamps.h"

#include "opencl/source/command_queue/gpgpu_walker_base.inl"
#include "opencl/source/command_queue/hardware_interface_base.inl"

namespace NEO {

using Family = Gen12LpFamily;

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::getDefaultDshSpace(
    const size_t &offsetInterfaceDescriptorTable,
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    size_t &totalInterfaceDescriptorTableSize,
    IndirectHeap *dsh,
    LinearStream *commandStream) {

    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    dsh->getSpace(totalInterfaceDescriptorTableSize);
}

template <typename GfxFamily>
template <typename WalkerType>
inline void HardwareInterface<GfxFamily>::programWalker(
    LinearStream &commandStream,
    Kernel &kernel,
    CommandQueue &commandQueue,
    IndirectHeap &dsh,
    IndirectHeap &ioh,
    IndirectHeap &ssh,
    const DispatchInfo &dispatchInfo,
    HardwareInterfaceWalkerArgs &walkerArgs) {

    auto walkerCmdBuf = allocateWalkerSpace<WalkerType>(commandStream, kernel);
    WalkerType walkerCmd = GfxFamily::cmdInitGpgpuWalker;
    uint32_t dim = dispatchInfo.getDim();
    uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();
    auto &rootDeviceEnvironment = commandQueue.getDevice().getRootDeviceEnvironment();

    size_t startWorkGroups[3] = {walkerArgs.startOfWorkgroups->x, walkerArgs.startOfWorkgroups->y, walkerArgs.startOfWorkgroups->z};
    size_t numWorkGroups[3] = {walkerArgs.numberOfWorkgroups->x, walkerArgs.numberOfWorkgroups->y, walkerArgs.numberOfWorkgroups->z};
    auto threadGroupCount = static_cast<uint32_t>(walkerArgs.numberOfWorkgroups->x * walkerArgs.numberOfWorkgroups->y * walkerArgs.numberOfWorkgroups->z);

    if (walkerArgs.currentTimestampPacketNodes && walkerArgs.currentTimestampPacketNodes->peekNodes().size() > 0 &&
        commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacketNode = walkerArgs.currentTimestampPacketNodes->peekNodes().at(walkerArgs.currentDispatchIndex);
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, &walkerCmd, timestampPacketNode, rootDeviceEnvironment);
    }

    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);

    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(&walkerCmd, kernel.getKernelInfo().kernelDescriptor,
                                                           startWorkGroups,
                                                           numWorkGroups, walkerArgs.localWorkSizes, simd, dim,
                                                           false, false, 0u);

    HardwareCommandsHelper<GfxFamily>::template sendIndirectState<WalkerType, typename HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        kernel,
        kernel.getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        simd,
        walkerArgs.localWorkSizes,
        threadGroupCount,
        walkerArgs.offsetInterfaceDescriptorTable,
        walkerArgs.interfaceDescriptorIndex,
        walkerArgs.preemptionMode,
        &walkerCmd,
        nullptr,
        kernelUsesLocalIds,
        0,
        commandQueue.getDevice());

    EncodeWalkerArgs encodeWalkerArgs{
        .kernelExecutionType = kernel.getExecutionType(),
        .requiredDispatchWalkOrder = RequiredDispatchWalkOrder::none,
        .localRegionSize = 0,
        .maxFrontEndThreads = 0,
        .requiredSystemFence = false,
        .hasSample = false};

    EncodeDispatchKernel<GfxFamily>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, encodeWalkerArgs);
    *walkerCmdBuf = walkerCmd;
}

template <typename GfxFamily>
template <typename WalkerType>
inline size_t GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(
    WalkerType *walkerCmd,
    const KernelDescriptor &kernelDescriptor,
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd,
    uint32_t workDim,
    bool localIdsGenerationByRuntime,
    bool inlineDataProgrammingRequired,
    uint32_t requiredWorkgroupOrder) {
    auto localWorkSize = static_cast<uint32_t>(localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2]);

    auto threadsPerWorkGroup = getThreadsPerWG(simd, localWorkSize);
    walkerCmd->setThreadWidthCounterMaximum(threadsPerWorkGroup);

    walkerCmd->setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
    walkerCmd->setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
    walkerCmd->setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));

    // compute executionMask - to tell which SIMD lines are active within thread
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = maxNBitValue(remainderSimdLanes);
    if (!executionMask)
        executionMask = ~executionMask;

    walkerCmd->setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd->setBottomExecutionMask(static_cast<uint32_t>(0xffffffff));
    walkerCmd->setSimdSize(getSimdConfig<DefaultWalkerType>(simd));

    walkerCmd->setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroups[0]));
    walkerCmd->setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroups[1]));
    walkerCmd->setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroups[2]));

    return localWorkSize;
}

template <typename GfxFamily>
template <typename WalkerType>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(
    LinearStream *cmdStream,
    WalkerType *walkerCmd,
    TagNodeBase *timestampPacketNode,
    const RootDeviceEnvironment &rootDeviceEnvironment) {

    uint64_t address = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *cmdStream,
        PostSyncMode::immediateData,
        address,
        0,
        rootDeviceEnvironment,
        args);
}

template <typename GfxFamily>
template <typename WalkerType>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                  sizeof(PIPE_CONTROL) * (MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(commandQueue.getDevice().getRootDeviceEnvironment()) ? 2 : 1);
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    size += PerformanceCounters::getGpuCommandsSize(commandQueue.getPerfCounters(), commandQueue.getGpgpuEngine().osContext->getEngineType(), reservePerfCounters);
    size += GpgpuWalkerHelper<GfxFamily>::getSizeForWaDisableRccRhwoOptimization(pKernel);

    return size;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite() {
    return sizeof(PIPE_CONTROL);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(
    TagNodeBase &hwTimeStamps,
    LinearStream *commandStream,
    const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, globalStartTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *commandStream,
        PostSyncMode::timestamp,
        timeStampAddress,
        0llu,
        rootDeviceEnvironment,
        args);

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
        // MI_STORE_REGISTER_MEM for context local timestamp
        timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, contextStartTS);

        // low part
        auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        MI_STORE_REGISTER_MEM cmd = GfxFamily::cmdInitStoreRegisterMem;
        adjustMiStoreRegMemMode(&cmd);
        cmd.setRegisterAddress(RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
        cmd.setMemoryAddress(timeStampAddress);
        *pMICmdLow = cmd;
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(
    TagNodeBase &hwTimeStamps,
    LinearStream *commandStream,
    const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, globalEndTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *commandStream,
        PostSyncMode::timestamp,
        timeStampAddress,
        0llu,
        rootDeviceEnvironment,
        args);

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
        // MI_STORE_REGISTER_MEM for context local timestamp
        uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, contextEndTS);

        // low part
        auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        MI_STORE_REGISTER_MEM cmd = GfxFamily::cmdInitStoreRegisterMem;
        adjustMiStoreRegMemMode(&cmd);
        cmd.setRegisterAddress(RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
        cmd.setMemoryAddress(timeStampAddress);
        *pMICmdLow = cmd;
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(const Kernel &kernel, const CommandQueue &commandQueue) {
    return 0;
}

template <>
void GpgpuWalkerHelper<Family>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<Family> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template <>
void HardwareInterface<Family>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {

    using PIPE_CONTROL = typename Family::PIPE_CONTROL;

    if (kernel.requiresWaDisableRccRhwoOptimization()) {

        PIPE_CONTROL cmdPipeControl = Family::cmdInitPipeControl;
        cmdPipeControl.setCommandStreamerStallEnable(true);
        auto pCmdPipeControl = commandStream->getSpaceForCmd<PIPE_CONTROL>();
        *pCmdPipeControl = cmdPipeControl;

        uint32_t value = enable ? 0x40004000 : 0x40000000;
        NEO::LriHelper<Family>::program(commandStream,
                                        0x7010,
                                        value,
                                        false,
                                        commandQueue.isBcs());
    }
}

template <>
size_t GpgpuWalkerHelper<Family>::getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel) {
    if (pKernel->requiresWaDisableRccRhwoOptimization()) {
        return (2 * (sizeof(Gen12LpFamily::PIPE_CONTROL) + sizeof(Family::MI_LOAD_REGISTER_IMM)));
    }
    return 0u;
}

template class HardwareInterface<Family>;

template void HardwareInterface<Family>::dispatchWalker<Family::DefaultWalkerType>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::DefaultWalkerType>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::DefaultWalkerType>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template Family::DefaultWalkerType *HardwareInterface<Family>::allocateWalkerSpace<Family::DefaultWalkerType>(LinearStream &commandStream, const Kernel &kernel);

template class GpgpuWalkerHelper<Family>;
template void GpgpuWalkerHelper<Family>::setupTimestampPacketFlushL3<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, CommandQueue &commandQueue, const FlushL3Args &args);
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::DefaultWalkerType>(LinearStream *cmdStream, Family::DefaultWalkerType *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t startWorkGroups[3],
                                                                                               const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);

template struct EnqueueOperation<Family>;

} // namespace NEO
