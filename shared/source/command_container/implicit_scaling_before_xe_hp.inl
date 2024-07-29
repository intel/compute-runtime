/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
template <typename WalkerType>
size_t ImplicitScalingDispatch<GfxFamily>::getSize(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount) {
    return 0;
}

template <typename GfxFamily>
template <typename WalkerType>
void ImplicitScalingDispatch<GfxFamily>::dispatchCommands(LinearStream &commandStream, WalkerType &walkerCmd, const DeviceBitfield &devices, ImplicitScalingDispatchCommandArgs &dispatchCommandArgs) {
}

template <typename GfxFamily>
bool &ImplicitScalingDispatch<GfxFamily>::getPipeControlStallRequired() {
    return ImplicitScalingDispatch<GfxFamily>::pipeControlStallRequired;
}

template <typename GfxFamily>
size_t ImplicitScalingDispatch<GfxFamily>::getBarrierSize(const RootDeviceEnvironment &rootDeviceEnvironment, bool apiSelfCleanup, bool usePostSync) {
    return 0;
}

template <typename GfxFamily>
void ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(LinearStream &commandStream, const DeviceBitfield &devices, PipeControlArgs &flushArgs, const RootDeviceEnvironment &rootDeviceEnvironment,
                                                                 uint64_t gpuAddress, uint64_t immediateData, bool apiSelfCleanup, bool useSecondaryBatchBuffer) {
}

template <typename GfxFamily>
inline size_t ImplicitScalingDispatch<GfxFamily>::getRegisterConfigurationSize() {
    return 0;
}

template <typename GfxFamily>
inline void ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(LinearStream &commandStream, uint64_t workPartitionSurfaceAddress, uint32_t addressOffset, bool isBcs) {
}

template <typename GfxFamily>
inline size_t ImplicitScalingDispatch<GfxFamily>::getOffsetRegisterSize() {
    return 0;
}

template <typename GfxFamily>
inline void ImplicitScalingDispatch<GfxFamily>::dispatchOffsetRegister(LinearStream &commandStream, uint32_t addressOffset, bool isBcs) {
}

template <typename GfxFamily>
inline uint32_t ImplicitScalingDispatch<GfxFamily>::getImmediateWritePostSyncOffset() {
    return sizeof(uint64_t);
}

template <typename GfxFamily>
inline uint32_t ImplicitScalingDispatch<GfxFamily>::getTimeStampPostSyncOffset() {
    return static_cast<uint32_t>(GfxCoreHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw());
}

template <typename GfxFamily>
inline bool ImplicitScalingDispatch<GfxFamily>::platformSupportsImplicitScaling(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}
template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = true;

template struct ImplicitScalingDispatch<Family>;
template void ImplicitScalingDispatch<Family>::dispatchCommands<Family::DefaultWalkerType>(LinearStream &commandStream, Family::DefaultWalkerType &walkerCmd, const DeviceBitfield &devices, ImplicitScalingDispatchCommandArgs &dispatchCommandArgs);
template size_t ImplicitScalingDispatch<Family>::getSize<Family::DefaultWalkerType>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);

} // namespace NEO
