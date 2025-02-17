/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/registered_method_dispatcher.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/utilities/stackvec.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/kernel/kernel_objects_for_aux_translation.h"

#include <algorithm>
#include <memory>

namespace NEO {
class LinearStream;

class Kernel;
class ClDevice;
struct TimestampPacketDependencies;

class DispatchInfo {

  public:
    using DispatchCommandMethodT = void(LinearStream &commandStream, TimestampPacketDependencies *timestampPacketDependencies, const RootDeviceEnvironment &rootDeviceEnvironment);
    using EstimateCommandsMethodT = size_t(size_t, const RootDeviceEnvironment &rootDeviceEnvironment, bool);

    DispatchInfo() = default;
    DispatchInfo(ClDevice *device, Kernel *kernel, uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset)
        : pClDevice(device), kernel(kernel), dim(dim), gws(gws), elws(elws), offset(offset) {}
    DispatchInfo(ClDevice *device, Kernel *kernel, uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset, const Vec3<size_t> &agws, const Vec3<size_t> &lws, const Vec3<size_t> &twgs, const Vec3<size_t> &nwgs, const Vec3<size_t> &swgs)
        : pClDevice(device), kernel(kernel), dim(dim), gws(gws), elws(elws), offset(offset), agws(agws), lws(lws), twgs(twgs), nwgs(nwgs), swgs(swgs) {}

    ClDevice &getClDevice() const { return *pClDevice; }
    void setClDevice(ClDevice *device) { pClDevice = device; }
    bool usesSlm() const;
    bool usesStatelessPrintfSurface() const;
    uint32_t getRequiredScratchSize(uint32_t slotId) const;
    void setKernel(Kernel *kernel) { this->kernel = kernel; }
    Kernel *getKernel() const { return kernel; }
    uint32_t getDim() const { return dim; }
    void setDim(uint32_t dim) { this->dim = dim; }
    const Vec3<size_t> &getGWS() const { return gws; };
    void setGWS(const Vec3<size_t> &gws) { this->gws = gws; }
    const Vec3<size_t> &getEnqueuedWorkgroupSize() const { return elws; };
    void setEnqueuedWorkgroupSize(const Vec3<size_t> &elws) { this->elws = elws; }
    const Vec3<size_t> &getOffset() const { return offset; };
    void setOffsets(const Vec3<size_t> &offset) { this->offset = offset; }
    const Vec3<size_t> &getActualWorkgroupSize() const { return agws; };
    void setActualGlobalWorkgroupSize(const Vec3<size_t> &agws) { this->agws = agws; }
    const Vec3<size_t> &getLocalWorkgroupSize() const { return lws; };
    void setLWS(const Vec3<size_t> &lws) { this->lws = lws; }
    const Vec3<size_t> &getTotalNumberOfWorkgroups() const { return twgs; };
    void setTotalNumberOfWorkgroups(const Vec3<size_t> &twgs) { this->twgs = twgs; }
    const Vec3<size_t> &getNumberOfWorkgroups() const { return nwgs; };
    void setNumberOfWorkgroups(const Vec3<size_t> &nwgs) { this->nwgs = nwgs; }
    const Vec3<size_t> &getStartOfWorkgroups() const { return swgs; };
    void setStartOfWorkgroups(const Vec3<size_t> &swgs) { this->swgs = swgs; }
    bool peekCanBePartitioned() const { return canBePartitioned; }
    void setCanBePartitioned(bool canBePartitioned) { this->canBePartitioned = canBePartitioned; }

    RegisteredMethodDispatcher<DispatchCommandMethodT, EstimateCommandsMethodT> dispatchInitCommands{};
    RegisteredMethodDispatcher<DispatchCommandMethodT, EstimateCommandsMethodT> dispatchEpilogueCommands{};

  protected:
    ClDevice *pClDevice = nullptr;
    bool canBePartitioned = false;
    Kernel *kernel = nullptr;
    uint32_t dim = 0;

    Vec3<size_t> gws{0, 0, 0};    // global work size
    Vec3<size_t> elws{0, 0, 0};   // enqueued local work size
    Vec3<size_t> offset{0, 0, 0}; // global offset
    Vec3<size_t> agws{0, 0, 0};   // actual global work size
    Vec3<size_t> lws{0, 0, 0};    // local work size
    Vec3<size_t> twgs{0, 0, 0};   // total number of work groups
    Vec3<size_t> nwgs{0, 0, 0};   // number of work groups
    Vec3<size_t> swgs{0, 0, 0};   // start of work groups
};

struct MultiDispatchInfo : NEO::NonCopyableAndNonMovableClass {
    ~MultiDispatchInfo();

    explicit MultiDispatchInfo(Kernel *mainKernel) : mainKernel(mainKernel) {}
    explicit MultiDispatchInfo(const BuiltinOpParams &operationParams) : builtinOpParams(operationParams) {}
    MultiDispatchInfo() = default;

    bool empty() const {
        return dispatchInfos.size() == 0;
    }

    bool usesSlm() const {
        for (const auto &dispatchInfo : dispatchInfos) {
            if (dispatchInfo.usesSlm()) {
                return true;
            }
        }
        return false;
    }

    bool usesStatelessPrintfSurface() const {
        for (const auto &dispatchInfo : dispatchInfos) {
            if (dispatchInfo.usesStatelessPrintfSurface()) {
                return true;
            }
        }
        return false;
    }

    uint32_t getRequiredScratchSize(uint32_t slotId) const {
        uint32_t ret = 0;
        for (const auto &dispatchInfo : dispatchInfos) {
            ret = std::max(ret, dispatchInfo.getRequiredScratchSize(slotId));
        }
        return ret;
    }

    void backupUnifiedMemorySyncRequirement();

    DispatchInfo *begin() {
        return dispatchInfos.begin();
    }

    const DispatchInfo *begin() const {
        return dispatchInfos.begin();
    }

    std::reverse_iterator<DispatchInfo *> rbegin() {
        return dispatchInfos.rbegin();
    }

    std::reverse_iterator<const DispatchInfo *> crbegin() const {
        return dispatchInfos.crbegin();
    }

    DispatchInfo *end() {
        return dispatchInfos.end();
    }

    const DispatchInfo *end() const {
        return dispatchInfos.end();
    }

    std::reverse_iterator<DispatchInfo *> rend() {
        return dispatchInfos.rend();
    }

    std::reverse_iterator<const DispatchInfo *> crend() const {
        return dispatchInfos.crend();
    }

    void push(const DispatchInfo &dispatchInfo) {
        dispatchInfos.push_back(dispatchInfo);
    }

    size_t size() const {
        return dispatchInfos.size();
    }

    StackVec<MemObj *, 2> &getRedescribedSurfaces() {
        return redescribedSurfaces;
    }

    void pushRedescribedMemObj(std::unique_ptr<MemObj> memObj);

    Kernel *peekMainKernel() const;

    void setBuiltinOpParams(const BuiltinOpParams &builtinOpParams) {
        this->builtinOpParams = builtinOpParams;
    }

    const BuiltinOpParams &peekBuiltinOpParams() const {
        return builtinOpParams;
    }

    void setKernelObjsForAuxTranslation(std::unique_ptr<KernelObjsForAuxTranslation> &&kernelObjsForAuxTranslation) {
        this->kernelObjsForAuxTranslation = std::move(kernelObjsForAuxTranslation);
    }

    const KernelObjsForAuxTranslation *getKernelObjsForAuxTranslation() const {
        return kernelObjsForAuxTranslation.get();
    }

  protected:
    BuiltinOpParams builtinOpParams = {};
    StackVec<DispatchInfo, 9> dispatchInfos;
    StackVec<MemObj *, 2> redescribedSurfaces;
    std::unique_ptr<const KernelObjsForAuxTranslation> kernelObjsForAuxTranslation;
    Kernel *mainKernel = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<MultiDispatchInfo>);

} // namespace NEO
