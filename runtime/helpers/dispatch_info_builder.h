/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {

namespace SplitDispatch {
enum class Dim : uint32_t {
    d1D = 0,
    d2D = 1,
    d3D = 2
};

enum class SplitMode : uint32_t {
    NoSplit = 0,
    WalkerSplit = 1, // 1 kernel and many GPGPU walkers (e.g. for non-uniform workgroup sizes)
    KernelSplit = 2  // many kernels and many GPGPU walkers (e.g. for copy kernels)
};

// Left | Middle | Right
enum class RegionCoordX : uint32_t {
    Left = 0,
    Middle = 1,
    Right = 2
};

//  Top
// ------
// Middle
// ------
// Bottom
enum class RegionCoordY : uint32_t {
    Top = 0,
    Middle = 1,
    Bottom = 2
};

//  Front  /        /
//        / Middle /
//       /        / Back
enum class RegionCoordZ : uint32_t {
    Front = 0,
    Middle = 1,
    Back = 2
};
} // namespace SplitDispatch

// Compute power in compile time
static constexpr uint32_t powConst(uint32_t base, uint32_t currExp) {
    return (currExp == 1) ? base : base * powConst(base, currExp - 1);
}

template <SplitDispatch::Dim Dim, SplitDispatch::SplitMode Mode>
class DispatchInfoBuilder {
  public:
    DispatchInfoBuilder() = default;

    void setKernel(Kernel *kernel) {
        for (auto &dispatchInfo : dispatchInfos) {
            dispatchInfo.setKernel(kernel);
        }
    }

    cl_int setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc) {
        for (auto &dispatchInfo : dispatchInfos) {
            if (dispatchInfo.getKernel()) {
                dispatchInfo.getKernel()->setArgSvmAlloc(argIndex, svmPtr, svmAlloc);
            }
        }
        return CL_SUCCESS;
    }

    template <typename... ArgsT>
    cl_int setArgSvm(ArgsT &&... args) {
        for (auto &dispatchInfo : dispatchInfos) {
            if (dispatchInfo.getKernel()) {
                dispatchInfo.getKernel()->setArgSvm(std::forward<ArgsT>(args)...);
            }
        }
        return CL_SUCCESS;
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d1D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setArgSvm(SplitDispatch::RegionCoordX x, ArgsT &&... args) {
        dispatchInfos[getDispatchId(x)].getKernel()->setArgSvm(std::forward<ArgsT>(args)...);
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d2D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setArgSvm(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, ArgsT &&... args) {
        dispatchInfos[getDispatchId(x, y)].getKernel()->setArgSvm(std::forward<ArgsT>(args)...);
    }
    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d3D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setArgSvm(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, SplitDispatch::RegionCoordZ z, ArgsT &&... args) {
        dispatchInfos[getDispatchId(x, y, z)].getKernel()->setArgSvm(std::forward<ArgsT>(args)...);
    }

    template <typename... ArgsT>
    cl_int setArg(ArgsT &&... args) {
        cl_int result = CL_SUCCESS;
        for (auto &dispatchInfo : dispatchInfos) {
            if (dispatchInfo.getKernel()) {
                result = dispatchInfo.getKernel()->setArg(std::forward<ArgsT>(args)...);
                if (result != CL_SUCCESS) {
                    break;
                }
            }
        }
        return result;
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d1D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setArg(SplitDispatch::RegionCoordX x, ArgsT &&... args) {
        dispatchInfos[getDispatchId(x)].getKernel()->setArg(std::forward<ArgsT>(args)...);
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d2D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setArg(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, ArgsT &&... args) {
        dispatchInfos[getDispatchId(x, y)].getKernel()->setArg(std::forward<ArgsT>(args)...);
    }
    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d3D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setArg(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, SplitDispatch::RegionCoordZ z, ArgsT &&... args) {
        dispatchInfos[getDispatchId(x, y, z)].getKernel()->setArg(std::forward<ArgsT>(args)...);
    }
    template <SplitDispatch::Dim D = Dim>
    typename std::enable_if<(D == SplitDispatch::Dim::d1D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setKernel(SplitDispatch::RegionCoordX x, Kernel *kern) {
        dispatchInfos[getDispatchId(x)].setKernel(kern);
    }

    template <SplitDispatch::Dim D = Dim>
    typename std::enable_if<(D == SplitDispatch::Dim::d2D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setKernel(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, Kernel *kern) {
        dispatchInfos[getDispatchId(x, y)].setKernel(kern);
    }

    template <SplitDispatch::Dim D = Dim>
    typename std::enable_if<(D == SplitDispatch::Dim::d3D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setKernel(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, SplitDispatch::RegionCoordZ z, Kernel *kern) {
        dispatchInfos[getDispatchId(x, y, z)].setKernel(kern);
    }

    template <SplitDispatch::SplitMode M = Mode>
    typename std::enable_if<(M == SplitDispatch::SplitMode::NoSplit) || (M == SplitDispatch::SplitMode::WalkerSplit), void>::type
    setDispatchGeometry(const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset, const Vec3<size_t> &agws = {0, 0, 0}, const Vec3<size_t> &lws = {0, 0, 0}, const Vec3<size_t> &twgs = {0, 0, 0}, const Vec3<size_t> &nwgs = {0, 0, 0}, const Vec3<size_t> &swgs = {0, 0, 0}) {
        auto &dispatchInfo = dispatchInfos[0];
        DEBUG_BREAK_IF(dim > static_cast<uint32_t>(Dim) + 1);
        dispatchInfo.setDim(dim);
        dispatchInfo.setGWS(gws);
        dispatchInfo.setEnqueuedWorkgroupSize(elws);
        dispatchInfo.setOffsets(offset);
        dispatchInfo.setActualGlobalWorkgroupSize(agws);
        dispatchInfo.setLWS(lws);
        dispatchInfo.setTotalNumberOfWorkgroups(twgs);
        dispatchInfo.setNumberOfWorkgroups(nwgs);
        dispatchInfo.setStartOfWorkgroups(swgs);
    }

    template <SplitDispatch::SplitMode M = Mode>
    typename std::enable_if<(M == SplitDispatch::SplitMode::NoSplit) || (M == SplitDispatch::SplitMode::WalkerSplit), void>::type
    setDispatchGeometry(const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset, const Vec3<size_t> &agws = {0, 0, 0}, const Vec3<size_t> &lws = {0, 0, 0}, const Vec3<size_t> &twgs = {0, 0, 0}, const Vec3<size_t> &nwgs = {0, 0, 0}, const Vec3<size_t> &swgs = {0, 0, 0}) {
        auto &dispatchInfo = dispatchInfos[0];
        dispatchInfo.setDim(static_cast<uint32_t>(Dim) + 1);
        dispatchInfo.setGWS(gws);
        dispatchInfo.setEnqueuedWorkgroupSize(elws);
        dispatchInfo.setOffsets(offset);
        dispatchInfo.setActualGlobalWorkgroupSize(agws);
        dispatchInfo.setLWS(lws);
        dispatchInfo.setTotalNumberOfWorkgroups(twgs);
        dispatchInfo.setNumberOfWorkgroups(nwgs);
        dispatchInfo.setStartOfWorkgroups(swgs);
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d1D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setDispatchGeometry(SplitDispatch::RegionCoordX x,
                        const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset, const Vec3<size_t> &agws = {0, 0, 0}, const Vec3<size_t> &lws = {0, 0, 0}, const Vec3<size_t> &twgs = {0, 0, 0}, const Vec3<size_t> &nwgs = {0, 0, 0}, const Vec3<size_t> &swgs = {0, 0, 0}) {
        auto &dispatchInfo = dispatchInfos[getDispatchId(x)];
        dispatchInfo.setDim(static_cast<uint32_t>(Dim) + 1);
        dispatchInfo.setGWS(gws);
        dispatchInfo.setEnqueuedWorkgroupSize(elws);
        dispatchInfo.setOffsets(offset);
        dispatchInfo.setActualGlobalWorkgroupSize(agws);
        dispatchInfo.setLWS(lws);
        dispatchInfo.setTotalNumberOfWorkgroups(twgs);
        dispatchInfo.setNumberOfWorkgroups(nwgs);
        dispatchInfo.setStartOfWorkgroups(swgs);
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d2D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setDispatchGeometry(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y,
                        const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset, const Vec3<size_t> &agws = {0, 0, 0}, const Vec3<size_t> lws = {0, 0, 0}, const Vec3<size_t> &twgs = {0, 0, 0}, const Vec3<size_t> &nwgs = {0, 0, 0}, const Vec3<size_t> &swgs = {0, 0, 0}) {
        auto &dispatchInfo = dispatchInfos[getDispatchId(x, y)];
        dispatchInfo.setDim(static_cast<uint32_t>(Dim) + 1);
        dispatchInfo.setGWS(gws);
        dispatchInfo.setEnqueuedWorkgroupSize(elws);
        dispatchInfo.setOffsets(offset);
        dispatchInfo.setActualGlobalWorkgroupSize(agws);
        dispatchInfo.setLWS(lws);
        dispatchInfo.setTotalNumberOfWorkgroups(twgs);
        dispatchInfo.setNumberOfWorkgroups(nwgs);
        dispatchInfo.setStartOfWorkgroups(swgs);
    }

    template <SplitDispatch::Dim D = Dim, typename... ArgsT>
    typename std::enable_if<(D == SplitDispatch::Dim::d3D) && (Mode != SplitDispatch::SplitMode::NoSplit), void>::type
    setDispatchGeometry(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, SplitDispatch::RegionCoordZ z,
                        const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset, const Vec3<size_t> &agws = {0, 0, 0}, const Vec3<size_t> &lws = {0, 0, 0}, const Vec3<size_t> &twgs = {0, 0, 0}, const Vec3<size_t> &nwgs = {0, 0, 0}, const Vec3<size_t> &swgs = {0, 0, 0}) {
        auto &dispatchInfo = dispatchInfos[getDispatchId(x, y, z)];
        dispatchInfo.setDim(static_cast<uint32_t>(Dim) + 1);
        dispatchInfo.setGWS(gws);
        dispatchInfo.setEnqueuedWorkgroupSize(elws);
        dispatchInfo.setOffsets(offset);
        dispatchInfo.setActualGlobalWorkgroupSize(agws);
        dispatchInfo.setLWS(lws);
        dispatchInfo.setTotalNumberOfWorkgroups(twgs);
        dispatchInfo.setNumberOfWorkgroups(nwgs);
        dispatchInfo.setStartOfWorkgroups(swgs);
    }

    void bake(MultiDispatchInfo &target) {
        for (auto &dispatchInfo : dispatchInfos) {
            if (((dispatchInfo.getDim() == 1) && (dispatchInfo.getGWS().x > 0)) ||
                ((dispatchInfo.getDim() == 2) && (dispatchInfo.getGWS().x > 0) && (dispatchInfo.getGWS().y > 0)) ||
                ((dispatchInfo.getDim() == 3) && (dispatchInfo.getGWS().x > 0) && (dispatchInfo.getGWS().y > 0) && (dispatchInfo.getGWS().z > 0))) {
                dispatchInfo.setDim(calculateDispatchDim(dispatchInfo.getGWS(), dispatchInfo.getOffset()));
                dispatchInfo.setGWS(canonizeWorkgroup(dispatchInfo.getGWS()));
                if (dispatchInfo.getActualWorkgroupSize() == Vec3<size_t>({0, 0, 0})) {
                    dispatchInfo.setActualGlobalWorkgroupSize(dispatchInfo.getGWS());
                }
                if (((dispatchInfo.getDim() == 1) && (dispatchInfo.getActualWorkgroupSize().x > 0)) ||
                    ((dispatchInfo.getDim() == 2) && (dispatchInfo.getActualWorkgroupSize().x > 0) && (dispatchInfo.getActualWorkgroupSize().y > 0)) ||
                    ((dispatchInfo.getDim() == 3) && (dispatchInfo.getActualWorkgroupSize().x > 0) && (dispatchInfo.getActualWorkgroupSize().y > 0) && (dispatchInfo.getActualWorkgroupSize().z > 0))) {
                    dispatchInfo.setEnqueuedWorkgroupSize(canonizeWorkgroup(dispatchInfo.getEnqueuedWorkgroupSize()));
                    if (dispatchInfo.getLocalWorkgroupSize().x == 0) {
                        dispatchInfo.setLWS(generateWorkgroupSize(dispatchInfo));
                    }
                    dispatchInfo.setLWS(canonizeWorkgroup(dispatchInfo.getLocalWorkgroupSize()));
                    if (dispatchInfo.getTotalNumberOfWorkgroups().x == 0) {
                        dispatchInfo.setTotalNumberOfWorkgroups(generateWorkgroupsNumber(dispatchInfo));
                    }
                    dispatchInfo.setTotalNumberOfWorkgroups(canonizeWorkgroup(dispatchInfo.getTotalNumberOfWorkgroups()));
                    if (dispatchInfo.getNumberOfWorkgroups().x == 0) {
                        dispatchInfo.setNumberOfWorkgroups(dispatchInfo.getTotalNumberOfWorkgroups());
                    }
                    if (supportsSplit() && needsSplit(dispatchInfo)) {
                        pushSplit(dispatchInfo, target);
                    } else {
                        target.push(dispatchInfo);
                        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stdout,
                                         "DIM:%u\tGWS:(%zu, %zu, %zu)\tELWS:(%zu, %zu, %zu)\tOffset:(%zu, %zu, %zu)\tAGWS:(%zu, %zu, %zu)\tLWS:(%zu, %zu, %zu)\tTWGS:(%zu, %zu, %zu)\tNWGS:(%zu, %zu, %zu)\tSWGS:(%zu, %zu, %zu)\n",
                                         dispatchInfo.getDim(),
                                         dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z,
                                         dispatchInfo.getEnqueuedWorkgroupSize().x, dispatchInfo.getEnqueuedWorkgroupSize().y, dispatchInfo.getEnqueuedWorkgroupSize().z,
                                         dispatchInfo.getOffset().x, dispatchInfo.getOffset().y, dispatchInfo.getOffset().z,
                                         dispatchInfo.getActualWorkgroupSize().x, dispatchInfo.getActualWorkgroupSize().y, dispatchInfo.getActualWorkgroupSize().z,
                                         dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getLocalWorkgroupSize().z,
                                         dispatchInfo.getTotalNumberOfWorkgroups().x, dispatchInfo.getTotalNumberOfWorkgroups().y, dispatchInfo.getTotalNumberOfWorkgroups().z,
                                         dispatchInfo.getNumberOfWorkgroups().x, dispatchInfo.getNumberOfWorkgroups().y, dispatchInfo.getNumberOfWorkgroups().z,
                                         dispatchInfo.getStartOfWorkgroups().x, dispatchInfo.getStartOfWorkgroups().y, dispatchInfo.getStartOfWorkgroups().z);
                    }
                }
            }
        }
    }

  protected:
    static bool supportsSplit() {
        return (Mode == SplitDispatch::SplitMode::WalkerSplit);
    }

    static bool needsSplit(const DispatchInfo &dispatchInfo) {
        return (dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x + dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y + dispatchInfo.getGWS().z % dispatchInfo.getLocalWorkgroupSize().z != 0);
    }

    static void pushSplit(const DispatchInfo &dispatchInfo, MultiDispatchInfo &outMdi) {
        constexpr auto xMain = SplitDispatch::RegionCoordX::Left;
        constexpr auto xRight = SplitDispatch::RegionCoordX::Middle;
        constexpr auto yMain = SplitDispatch::RegionCoordY::Top;
        constexpr auto yBottom = SplitDispatch::RegionCoordY::Middle;
        constexpr auto zMain = SplitDispatch::RegionCoordZ::Front;
        constexpr auto zBack = SplitDispatch::RegionCoordZ::Middle;

        switch (dispatchInfo.getDim()) {
        default:
            break;
        case 1: {
            Vec3<size_t> mainLWS = {dispatchInfo.getLocalWorkgroupSize().x, 1, 1};
            Vec3<size_t> rightLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, 1, 1};

            Vec3<size_t> mainGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), 1, 1};
            Vec3<size_t> rightGWS = {dispatchInfo.getGWS().x % mainLWS.x, 1, 1};

            Vec3<size_t> mainNWGS = {mainGWS.x / mainLWS.x, 1, 1};
            Vec3<size_t> rightNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), 1, 1};

            Vec3<size_t> mainSWGS = {0, 0, 0};
            Vec3<size_t> rightSWGS = {mainNWGS.x, 0, 0};

            DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::KernelSplit> builder1D;

            builder1D.setKernel(dispatchInfo.getKernel());

            builder1D.setDispatchGeometry(xMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), mainGWS, mainLWS, dispatchInfo.getTotalNumberOfWorkgroups(), mainNWGS, mainSWGS);
            builder1D.setDispatchGeometry(xRight, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightGWS, rightLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightNWGS, rightSWGS);

            builder1D.bake(outMdi);
        } break;
        case 2: {
            Vec3<size_t> mainLWS = {dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getLocalWorkgroupSize().y, 1};
            Vec3<size_t> rightLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getLocalWorkgroupSize().y, 1};
            Vec3<size_t> bottomLWS = {dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y, 1};
            Vec3<size_t> rightbottomLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y, 1};

            Vec3<size_t> mainGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), alignDown(dispatchInfo.getGWS().y, mainLWS.y), 1};
            Vec3<size_t> rightGWS = {dispatchInfo.getGWS().x % mainLWS.x, alignDown(dispatchInfo.getGWS().y, mainLWS.y), 1};
            Vec3<size_t> bottomGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), dispatchInfo.getGWS().y % mainLWS.y, 1};
            Vec3<size_t> rightbottomGWS = {dispatchInfo.getGWS().x % mainLWS.x, dispatchInfo.getGWS().y % mainLWS.y, 1};

            Vec3<size_t> mainNWGS = {mainGWS.x / mainLWS.x, mainGWS.y / mainLWS.y, 1};
            Vec3<size_t> rightNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), mainGWS.y / mainLWS.y, 1};
            Vec3<size_t> bottomNWGS = {mainGWS.x / mainLWS.x, mainNWGS.y + isIndivisible(bottomGWS.y, mainLWS.y), 1};
            Vec3<size_t> rightbottomNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), mainNWGS.y + isIndivisible(bottomGWS.y, mainLWS.y), 1};

            Vec3<size_t> mainSWGS = {0, 0, 0};
            Vec3<size_t> rightSWGS = {mainNWGS.x, 0, 0};
            Vec3<size_t> bottomSWGS = {0, mainNWGS.y, 0};
            Vec3<size_t> rightbottomSWGS = {mainNWGS.x, mainNWGS.y, 0};

            DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::KernelSplit> builder2D;

            builder2D.setKernel(dispatchInfo.getKernel());

            builder2D.setDispatchGeometry(xMain, yMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), mainGWS, mainLWS, dispatchInfo.getTotalNumberOfWorkgroups(), mainNWGS, mainSWGS);
            builder2D.setDispatchGeometry(xRight, yMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightGWS, rightLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightNWGS, rightSWGS);
            builder2D.setDispatchGeometry(xMain, yBottom, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), bottomGWS, bottomLWS, dispatchInfo.getTotalNumberOfWorkgroups(), bottomNWGS, bottomSWGS);
            builder2D.setDispatchGeometry(xRight, yBottom, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightbottomGWS, rightbottomLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightbottomNWGS, rightbottomSWGS);

            builder2D.bake(outMdi);
        } break;
        case 3: {
            Vec3<size_t> mainLWS = dispatchInfo.getLocalWorkgroupSize();
            Vec3<size_t> rightLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getLocalWorkgroupSize().z};
            Vec3<size_t> bottomLWS = {dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getLocalWorkgroupSize().z};
            Vec3<size_t> rightbottomLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getLocalWorkgroupSize().z};
            Vec3<size_t> mainbackLWS = {dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getGWS().z % dispatchInfo.getLocalWorkgroupSize().z};
            Vec3<size_t> rightbackLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getGWS().z % dispatchInfo.getLocalWorkgroupSize().z};
            Vec3<size_t> bottombackLWS = {dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getGWS().z % dispatchInfo.getLocalWorkgroupSize().z};
            Vec3<size_t> rightbottombackLWS = {dispatchInfo.getGWS().x % dispatchInfo.getLocalWorkgroupSize().x, dispatchInfo.getGWS().y % dispatchInfo.getLocalWorkgroupSize().y, dispatchInfo.getGWS().z % dispatchInfo.getLocalWorkgroupSize().z};

            Vec3<size_t> mainGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), alignDown(dispatchInfo.getGWS().y, mainLWS.y), alignDown(dispatchInfo.getGWS().z, mainLWS.z)};
            Vec3<size_t> rightGWS = {dispatchInfo.getGWS().x % mainLWS.x, alignDown(dispatchInfo.getGWS().y, mainLWS.y), alignDown(dispatchInfo.getGWS().z, mainLWS.z)};
            Vec3<size_t> bottomGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), dispatchInfo.getGWS().y % mainLWS.y, alignDown(dispatchInfo.getGWS().z, mainLWS.z)};
            Vec3<size_t> rightbottomGWS = {dispatchInfo.getGWS().x % mainLWS.x, dispatchInfo.getGWS().y % mainLWS.y, alignDown(dispatchInfo.getGWS().z, mainLWS.z)};
            Vec3<size_t> mainbackGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), alignDown(dispatchInfo.getGWS().y, mainLWS.y), dispatchInfo.getGWS().z % mainLWS.z};
            Vec3<size_t> rightbackGWS = {dispatchInfo.getGWS().x % mainLWS.x, alignDown(dispatchInfo.getGWS().y, mainLWS.y), dispatchInfo.getGWS().z % mainLWS.z};
            Vec3<size_t> bottombackGWS = {alignDown(dispatchInfo.getGWS().x, mainLWS.x), dispatchInfo.getGWS().y % mainLWS.y, dispatchInfo.getGWS().z % mainLWS.z};
            Vec3<size_t> rightbottombackGWS = {dispatchInfo.getGWS().x % mainLWS.x, dispatchInfo.getGWS().y % mainLWS.y, dispatchInfo.getGWS().z % mainLWS.z};

            Vec3<size_t> mainNWGS = {mainGWS.x / mainLWS.x, mainGWS.y / mainLWS.y, mainGWS.z / mainLWS.z + isIndivisible(mainGWS.z, mainLWS.z)};
            Vec3<size_t> rightNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), mainGWS.y / mainLWS.y, mainGWS.z / mainLWS.z};
            Vec3<size_t> bottomNWGS = {mainGWS.x / mainLWS.x, mainNWGS.y + isIndivisible(bottomGWS.y, mainLWS.y), mainGWS.z / mainLWS.z};
            Vec3<size_t> rightbottomNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), mainNWGS.y + isIndivisible(bottomGWS.y, mainLWS.y), mainGWS.z / mainLWS.z};
            Vec3<size_t> mainbackNWGS = {mainGWS.x / mainLWS.x, mainGWS.y / mainLWS.y, mainNWGS.z + isIndivisible(mainbackGWS.z, mainLWS.z)};
            Vec3<size_t> rightbackNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), mainGWS.y / mainLWS.y, mainNWGS.z + isIndivisible(rightbackGWS.z, mainLWS.z)};
            Vec3<size_t> bottombackNWGS = {mainGWS.x / mainLWS.x, mainNWGS.y + isIndivisible(bottomGWS.y, mainLWS.y), mainNWGS.z + isIndivisible(bottombackGWS.z, mainLWS.z)};
            Vec3<size_t> rightbottombackNWGS = {mainNWGS.x + isIndivisible(rightGWS.x, mainLWS.x), mainNWGS.y + isIndivisible(bottomGWS.y, mainLWS.y), mainNWGS.z + isIndivisible(rightbottombackGWS.z, mainLWS.z)};

            Vec3<size_t> mainSWGS = {0, 0, 0};
            Vec3<size_t> rightSWGS = {mainNWGS.x, 0, 0};
            Vec3<size_t> bottomSWGS = {0, mainNWGS.y, 0};
            Vec3<size_t> rightbottomSWGS = {mainNWGS.x, mainNWGS.y, 0};
            Vec3<size_t> mainbackSWGS = {0, 0, mainNWGS.z};
            Vec3<size_t> rightbackSWGS = {mainNWGS.x, 0, mainNWGS.z};
            Vec3<size_t> bottombackSWGS = {0, mainNWGS.y, mainNWGS.z};
            Vec3<size_t> rightbottombackSWGS = {mainNWGS.x, mainNWGS.y, mainNWGS.z};

            DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::KernelSplit> builder3D;

            builder3D.setKernel(dispatchInfo.getKernel());

            builder3D.setDispatchGeometry(xMain, yMain, zMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), mainGWS, mainLWS, dispatchInfo.getTotalNumberOfWorkgroups(), mainNWGS, mainSWGS);
            builder3D.setDispatchGeometry(xRight, yMain, zMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightGWS, rightLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightNWGS, rightSWGS);
            builder3D.setDispatchGeometry(xMain, yBottom, zMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), bottomGWS, bottomLWS, dispatchInfo.getTotalNumberOfWorkgroups(), bottomNWGS, bottomSWGS);
            builder3D.setDispatchGeometry(xRight, yBottom, zMain, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightbottomGWS, rightbottomLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightbottomNWGS, rightbottomSWGS);
            builder3D.setDispatchGeometry(xMain, yMain, zBack, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), mainbackGWS, mainbackLWS, dispatchInfo.getTotalNumberOfWorkgroups(), mainbackNWGS, mainbackSWGS);
            builder3D.setDispatchGeometry(xRight, yMain, zBack, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightbackGWS, rightbackLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightbackNWGS, rightbackSWGS);
            builder3D.setDispatchGeometry(xMain, yBottom, zBack, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), bottombackGWS, bottombackLWS, dispatchInfo.getTotalNumberOfWorkgroups(), bottombackNWGS, bottombackSWGS);
            builder3D.setDispatchGeometry(xRight, yBottom, zBack, dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize(), dispatchInfo.getOffset(), rightbottombackGWS, rightbottombackLWS, dispatchInfo.getTotalNumberOfWorkgroups(), rightbottombackNWGS, rightbottombackSWGS);

            builder3D.bake(outMdi);
        } break;
        }
    }

    static constexpr uint32_t getDispatchId(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y, SplitDispatch::RegionCoordZ z) {
        return static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * (static_cast<uint32_t>(Mode) + 1) + static_cast<uint32_t>(z) * (static_cast<uint32_t>(Mode) + 1) * (static_cast<uint32_t>(Mode) + 1);
    }

    static constexpr uint32_t getDispatchId(SplitDispatch::RegionCoordX x, SplitDispatch::RegionCoordY y) {
        return static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * (static_cast<uint32_t>(Mode) + 1);
    }

    static constexpr uint32_t getDispatchId(SplitDispatch::RegionCoordX x) {
        return static_cast<uint32_t>(x);
    }

    static constexpr size_t getMaxNumDispatches() {
        return numDispatches;
    }

    static const size_t numDispatches = (Mode == SplitDispatch::SplitMode::WalkerSplit) ? 1 : powConst((static_cast<uint32_t>(Mode) + 1), // 1 (middle) 2 (middle + right/bottom) or 3 (lef/top + middle + right/mottom)
                                                                                                       (static_cast<uint32_t>(Dim) + 1)); // 1, 2 or 3

    DispatchInfo dispatchInfos[numDispatches];

  private:
    static size_t alignDown(size_t x, size_t y) {
        return x - x % y;
    }

    static size_t isIndivisible(size_t x, size_t y) {
        return x % y ? 1 : 0;
    }
};
} // namespace OCLRT
