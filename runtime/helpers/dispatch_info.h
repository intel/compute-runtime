/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/memory_manager/surface.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/utilities/vec.h"
#include "runtime/utilities/stackvec.h"
#include <algorithm>
#include <memory>

namespace OCLRT {

class Kernel;

class DispatchInfo {
  public:
    DispatchInfo() : gws(0, 0, 0), elws(0, 0, 0), offset(0, 0, 0), agws(0, 0, 0), lws(0, 0, 0), twgs(0, 0, 0), nwgs(0, 0, 0), swgs(0, 0, 0) {}
    DispatchInfo(Kernel *k, uint32_t d, Vec3<size_t> gws, Vec3<size_t> elws, Vec3<size_t> offset)
        : kernel(k), dim(d), gws(gws), elws(elws), offset(offset), agws(0, 0, 0), lws(0, 0, 0), twgs(0, 0, 0), nwgs(0, 0, 0), swgs(0, 0, 0) {}
    DispatchInfo(Kernel *k, uint32_t d, Vec3<size_t> gws, Vec3<size_t> elws, Vec3<size_t> offset, Vec3<size_t> agws, Vec3<size_t> lws, Vec3<size_t> twgs, Vec3<size_t> nwgs, Vec3<size_t> swgs)
        : kernel(k), dim(d), gws(gws), elws(elws), offset(offset), agws(agws), lws(lws), twgs(twgs), nwgs(nwgs), swgs(swgs) {}
    bool usesSlm() const;
    bool usesStatelessPrintfSurface() const;
    uint32_t getRequiredScratchSize() const;
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

  protected:
    Kernel *kernel = nullptr;
    uint32_t dim = 0;

    Vec3<size_t> gws;    //global work size
    Vec3<size_t> elws;   //enqueued local work size
    Vec3<size_t> offset; //global offset
    Vec3<size_t> agws;   //actual global work size
    Vec3<size_t> lws;    //local work size
    Vec3<size_t> twgs;   //total number of work groups
    Vec3<size_t> nwgs;   //number of work groups
    Vec3<size_t> swgs;   //start of work groups
};

struct MultiDispatchInfo {
    ~MultiDispatchInfo() {
        for (MemObj *redescribedSurface : redescribedSurfaces) {
            redescribedSurface->release();
        }
    }

    MultiDispatchInfo(Kernel *mainKernel) : mainKernel(mainKernel) {}
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

    const DispatchInfo *begin() const {
        return dispatchInfos.begin();
    }

    const DispatchInfo *end() const {
        return dispatchInfos.end();
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

  protected:
    StackVec<DispatchInfo, 9> dispatchInfos;
    StackVec<MemObj *, 2> redescribedSurfaces;
    Kernel *mainKernel = nullptr;
};
} // namespace OCLRT
