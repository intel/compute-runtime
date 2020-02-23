/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/registered_method_dispatcher.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/utilities/stackvec.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/mem_obj/mem_obj.h"

#include <algorithm>
#include <memory>

namespace NEO {

class Kernel;
struct TimestampPacketDependencies;

class DispatchInfo {

  public:
    using DispatchCommandMethodT = void(LinearStream &commandStream, TimestampPacketDependencies *timestampPacketDependencies);
    using EstimateCommandsMethodT = size_t(size_t);

    DispatchInfo() = default;
    DispatchInfo(Kernel *kernel, uint32_t dim, Vec3<size_t> gws, Vec3<size_t> elws, Vec3<size_t> offset)
        : kernel(kernel), dim(dim), gws(gws), elws(elws), offset(offset) {}
    DispatchInfo(Kernel *kernel, uint32_t dim, Vec3<size_t> gws, Vec3<size_t> elws, Vec3<size_t> offset, Vec3<size_t> agws, Vec3<size_t> lws, Vec3<size_t> twgs, Vec3<size_t> nwgs, Vec3<size_t> swgs)
        : kernel(kernel), dim(dim), gws(gws), elws(elws), offset(offset), agws(agws), lws(lws), twgs(twgs), nwgs(nwgs), swgs(swgs) {}
    bool usesSlm() const;
    bool usesStatelessPrintfSurface() const;
    uint32_t getRequiredScratchSize() const;
    uint32_t getRequiredPrivateScratchSize() const;
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

    RegisteredMethodDispatcher<DispatchCommandMethodT, EstimateCommandsMethodT> dispatchInitCommands;
    RegisteredMethodDispatcher<DispatchCommandMethodT, EstimateCommandsMethodT> dispatchEpilogueCommands;

  protected:
    bool canBePartitioned = false;
    Kernel *kernel = nullptr;
    uint32_t dim = 0;

    Vec3<size_t> gws{0, 0, 0};    //global work size
    Vec3<size_t> elws{0, 0, 0};   //enqueued local work size
    Vec3<size_t> offset{0, 0, 0}; //global offset
    Vec3<size_t> agws{0, 0, 0};   //actual global work size
    Vec3<size_t> lws{0, 0, 0};    //local work size
    Vec3<size_t> twgs{0, 0, 0};   //total number of work groups
    Vec3<size_t> nwgs{0, 0, 0};   //number of work groups
    Vec3<size_t> swgs{0, 0, 0};   //start of work groups
};

struct MultiDispatchInfo {
    ~MultiDispatchInfo() {
        for (MemObj *redescribedSurface : redescribedSurfaces) {
            redescribedSurface->release();
        }
    }

    explicit MultiDispatchInfo(Kernel *mainKernel) : mainKernel(mainKernel) {}
    MultiDispatchInfo() = default;

    MultiDispatchInfo &operator=(const MultiDispatchInfo &) = delete;
    MultiDispatchInfo(const MultiDispatchInfo &) = delete;

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

    uint32_t getRequiredScratchSize() const {
        uint32_t ret = 0;
        for (const auto &dispatchInfo : dispatchInfos) {
            ret = std::max(ret, dispatchInfo.getRequiredScratchSize());
        }
        return ret;
    }

    uint32_t getRequiredPrivateScratchSize() const {
        uint32_t ret = 0;
        for (const auto &dispatchInfo : dispatchInfos) {
            ret = std::max(ret, dispatchInfo.getRequiredPrivateScratchSize());
        }
        return ret;
    }

    void backupUnifiedMemorySyncRequirement() {
        for (const auto &dispatchInfo : dispatchInfos) {
            dispatchInfo.getKernel()->setUnifiedMemorySyncRequirement(true);
        }
    }

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

    void pushRedescribedMemObj(std::unique_ptr<MemObj> memObj) {
        redescribedSurfaces.push_back(memObj.release());
    }

    Kernel *peekParentKernel() const;
    Kernel *peekMainKernel() const;

    void setBuiltinOpParams(const BuiltinOpParams &builtinOpParams) {
        this->builtinOpParams = builtinOpParams;
    }

    const BuiltinOpParams &peekBuiltinOpParams() const {
        return builtinOpParams;
    }

    void setMemObjsForAuxTranslation(const MemObjsForAuxTranslation &memObjsForAuxTranslation) {
        this->memObjsForAuxTranslation = &memObjsForAuxTranslation;
    }

    const MemObjsForAuxTranslation *getMemObjsForAuxTranslation() const {
        return memObjsForAuxTranslation;
    }

  protected:
    BuiltinOpParams builtinOpParams = {};
    StackVec<DispatchInfo, 9> dispatchInfos;
    StackVec<MemObj *, 2> redescribedSurfaces;
    const MemObjsForAuxTranslation *memObjsForAuxTranslation = nullptr;
    Kernel *mainKernel = nullptr;
};
} // namespace NEO
