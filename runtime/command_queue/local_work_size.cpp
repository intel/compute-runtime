/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>

namespace OCLRT {

//threshold used to determine what kind of device is underneath
//big cores like SKL have 8EU * 7 HW threads per subslice and are considered as highThreadCount devices
constexpr uint32_t highThreadCountThreshold = 56u;

static const uint32_t optimalHardwareThreadCountGeneric[] = {32, 16, 8, 4, 2, 1};

static const uint32_t primeNumbers[] = {
    251,
    241,
    239, 233,
    229, 227, 223,
    211,
    199, 197, 193, 191,
    181,
    179, 173,
    167, 163,
    157, 151,
    149,
    139, 137, 131,
    127,
    113,
    109, 107, 103, 101,
    97,
    89, 83,
    79, 73, 71,
    67, 61,
    59, 53,
    47, 43, 41,
    37, 31,
    29, 23,
    19, 17, 13, 11,
    7, 5, 3, 2};

static const size_t MAX_PRIMES = sizeof(primeNumbers) / sizeof(primeNumbers[0]);

// Recursive template function to test prime factors
template <uint32_t primeIndex>
static inline uint32_t factor(size_t workItems, uint32_t workSize, uint32_t maxWorkGroupSize) {
    auto primeNumber = primeNumbers[primeIndex];

    auto newWorkSize = workSize * primeNumber;
    if (newWorkSize <= workItems) {
        while (newWorkSize <= maxWorkGroupSize && (workItems % newWorkSize) == 0) {
            workSize = newWorkSize;
            newWorkSize = workSize * primeNumber;
        }

        workSize = factor<primeIndex - 1>(workItems, workSize, maxWorkGroupSize);
    }

    return workSize;
}

// Terminator of recursive factoring logic
template <>
inline uint32_t factor<0>(size_t workItems, uint32_t workSize, uint32_t maxWorkGroupSize) {
    uint32_t primeIndex = 0;
    auto primeNumber = primeNumbers[primeIndex];

    auto newWorkSize = workSize * primeNumber;
    if (newWorkSize <= workItems) {
        while (newWorkSize <= maxWorkGroupSize && (workItems % newWorkSize) == 0) {
            workSize = newWorkSize;
            newWorkSize = workSize * primeNumber;
        }
    }

    return workSize;
}

void computePowerOfTwoLWS(const size_t workItems[3], WorkSizeInfo &workGroupInfo, size_t workGroupSize[3], const uint32_t workDim, bool canUseNx4) {
    uint32_t targetIndex = (canUseNx4 || workGroupInfo.numThreadsPerSubSlice < highThreadCountThreshold) ? 2 : 0;
    auto arraySize = arrayCount(optimalHardwareThreadCountGeneric);
    auto simdSize = workGroupInfo.simdSize;

    while (targetIndex < arraySize &&
           optimalHardwareThreadCountGeneric[targetIndex] > 1 &&
           workGroupInfo.maxWorkGroupSize < optimalHardwareThreadCountGeneric[targetIndex] * simdSize) {
        targetIndex++;
    }
    uint32_t optimalLocalThreads = optimalHardwareThreadCountGeneric[targetIndex];

    if (workDim == 2) {
        uint32_t xDim, yDim;
        xDim = uint32_t(optimalLocalThreads * simdSize) / (canUseNx4 ? 4 : 1);
        while (xDim > workItems[0])
            xDim = xDim >> 1;
        yDim = canUseNx4 ? 4 : (uint32_t(optimalLocalThreads * simdSize) / xDim);
        workGroupSize[0] = xDim;
        workGroupSize[1] = yDim;
    } else {
        uint32_t xDim, yDim, zDim;
        xDim = uint32_t(optimalLocalThreads * simdSize);
        while (xDim > workItems[0])
            xDim = xDim >> 1;
        yDim = uint32_t(optimalLocalThreads * simdSize) / xDim;
        while (yDim > workItems[1])
            yDim = yDim >> 1;
        UNRECOVERABLE_IF((xDim * yDim) == 0);
        zDim = uint32_t(optimalLocalThreads * simdSize) / (xDim * yDim);
        workGroupSize[0] = xDim;
        workGroupSize[1] = yDim;
        workGroupSize[2] = zDim;
    }
}

void choosePreferredWorkGroupSizeWithRatio(uint32_t xyzFactors[3][1024], uint32_t xyzFactorsLen[3], size_t workGroupSize[3], const size_t workItems[3], WorkSizeInfo wsInfo) {
    float ratioDiff = 0;
    float localRatio = float(0xffffffff);
    ulong localWkgs = 0xffffffff;
    ulong workGroups;
    for (cl_uint XFactorsIdx = 0; XFactorsIdx < xyzFactorsLen[0]; ++XFactorsIdx) {
        for (cl_uint YFactorsIdx = 0; YFactorsIdx < xyzFactorsLen[1]; ++YFactorsIdx) {

            uint32_t Xdim = xyzFactors[0][xyzFactorsLen[0] - 1 - XFactorsIdx];
            uint32_t Ydim = xyzFactors[1][YFactorsIdx];

            if ((Xdim * Ydim) > wsInfo.maxWorkGroupSize) {
                break;
            }
            if ((Xdim * Ydim) < wsInfo.minWorkGroupSize) {
                continue;
            }

            workGroups = (workItems[0] + Xdim - 1) / Xdim;
            workGroups *= (workItems[1] + Ydim - 1) / Ydim;

            ratioDiff = log((float)Xdim) - log((float)Ydim);
            ratioDiff = fabs(wsInfo.targetRatio - ratioDiff);

            if (wsInfo.useStrictRatio == CL_TRUE) {
                if (ratioDiff < localRatio) {
                    workGroupSize[0] = Xdim;
                    workGroupSize[1] = Ydim;
                    localRatio = ratioDiff;
                    localWkgs = workGroups;
                }
            } else {
                if ((workGroups < localWkgs) ||
                    ((workGroups == localWkgs) && (ratioDiff < localRatio))) {
                    workGroupSize[0] = Xdim;
                    workGroupSize[1] = Ydim;
                    localRatio = ratioDiff;
                    localWkgs = workGroups;
                }
            }
        }
    }
}
void choosePreferredWorkGroupSizeWithOutRatio(uint32_t xyzFactors[3][1024], uint32_t xyzFactorsLen[3], size_t workGroupSize[3], const size_t workItems[3], WorkSizeInfo wsInfo, uint32_t workdim) {
    ulong localEuThrdsDispatched = 0xffffffff;
    ulong workGroups;
    for (uint32_t ZFactorsIdx = 0; ZFactorsIdx < xyzFactorsLen[2]; ++ZFactorsIdx) {
        for (uint32_t XFactorsIdx = 0; XFactorsIdx < xyzFactorsLen[0]; ++XFactorsIdx) {
            for (uint32_t YFactorsIdx = 0; YFactorsIdx < xyzFactorsLen[1]; ++YFactorsIdx) {

                uint32_t Xdim = xyzFactors[0][xyzFactorsLen[0] - 1 - XFactorsIdx];
                uint32_t Ydim = xyzFactors[1][YFactorsIdx];
                uint32_t Zdim = xyzFactors[2][ZFactorsIdx];

                if ((Xdim * Ydim * Zdim) > wsInfo.maxWorkGroupSize) {
                    break;
                }
                if ((Xdim * Ydim * Zdim) < wsInfo.minWorkGroupSize) {
                    continue;
                }

                workGroups = (workItems[0] + Xdim - 1) / Xdim;
                workGroups *= (workItems[1] + Ydim - 1) / Ydim;
                workGroups *= (workItems[2] + Zdim - 1) / Zdim;
                cl_ulong euThrdsDispatched;

                euThrdsDispatched = (Xdim * Ydim * Zdim + wsInfo.simdSize - 1) / wsInfo.simdSize;
                euThrdsDispatched *= workGroups;

                if (euThrdsDispatched < localEuThrdsDispatched) {
                    localEuThrdsDispatched = euThrdsDispatched;
                    workGroupSize[0] = Xdim;
                    workGroupSize[1] = Ydim;
                    workGroupSize[2] = Zdim;
                }
            }
        }
    }
}

void computeWorkgroupSize1D(uint32_t maxWorkGroupSize,
                            size_t workGroupSize[3],
                            const size_t workItems[3],
                            size_t simdSize) {
    auto items = workItems[0];

    // Determine the LSB set to quickly handle factors of 2
    auto numBits = Math::getMinLsbSet(static_cast<uint32_t>(items));

    // Clamp power of 2 result to maxWorkGroupSize
    uint32_t workSize = 1u << numBits;

    //Assumes maxWorkGroupSize is a power of two.
    DEBUG_BREAK_IF((maxWorkGroupSize & (maxWorkGroupSize - 1)) != 0);
    workSize = std::min(workSize, maxWorkGroupSize);

    // Try all primes as potential factors
    workSize = factor<MAX_PRIMES - 1>(items, workSize, maxWorkGroupSize);

    workGroupSize[0] = workSize;
    workGroupSize[1] = 1;
    workGroupSize[2] = 1;
}

void computeWorkgroupSize2D(uint32_t maxWorkGroupSize, size_t workGroupSize[3], const size_t workItems[3], size_t simdSize) {
    uint32_t xFactors[1024];
    uint32_t yFactors[1024];
    uint32_t xFactorsLen = 0;
    uint32_t yFactorsLen = 0;
    uint64_t waste;
    uint64_t localWSWaste = 0xffffffff;
    uint64_t euThrdsDispatched;
    uint64_t localEuThrdsDispatched = 0xffffffff;
    uint64_t workGroups;
    uint32_t xDim;
    uint32_t yDim;

    for (int i = 0; i < 3; i++)
        workGroupSize[i] = 1;

    for (uint32_t i = 2; i <= maxWorkGroupSize; i++) {
        if ((workItems[0] % i) == 0) {
            xFactors[xFactorsLen++] = i;
        }
        if (((workItems[1] % i) == 0)) {
            yFactors[yFactorsLen++] = i;
        }
    }

    for (uint32_t xFactorsIdx = 0; xFactorsIdx < xFactorsLen; ++xFactorsIdx) {
        for (uint32_t yFactorsIdx = 0; yFactorsIdx < yFactorsLen; ++yFactorsIdx) {
            // Pick a LocalWorkSize that is a multiple as well as appropriate:
            // 1 <= workGroupSize[ 0 ] <= workItems[ 0 ]
            // 1 <= workGroupSize[ 1 ] <= workItems[ 1 ]
            xDim = xFactors[xFactorsLen - 1 - xFactorsIdx];
            yDim = yFactors[yFactorsIdx];

            if ((xDim * yDim) > maxWorkGroupSize) {
                // The yDim value is too big, so break out of this loop.
                // No other entries will work.
                break;
            }

            // Find the wasted channels.
            workGroups = (workItems[0] + xDim - 1) / xDim;
            workGroups *= (workItems[1] + yDim - 1) / yDim;

            // Compaction Mode!
            euThrdsDispatched = (xDim * yDim + simdSize - 1) / simdSize;
            euThrdsDispatched *= workGroups;

            waste = simdSize - ((xDim * yDim - 1) & (simdSize - 1));
            waste *= workGroups;

            if (((euThrdsDispatched < localEuThrdsDispatched) ||
                 ((euThrdsDispatched == localEuThrdsDispatched) && (waste < localWSWaste)))) {
                localWSWaste = waste;
                localEuThrdsDispatched = euThrdsDispatched;
                workGroupSize[0] = xDim;
                workGroupSize[1] = yDim;
            }
        }
    }
}

void computeWorkgroupSizeSquared(uint32_t maxWorkGroupSize, size_t workGroupSize[3], const size_t workItems[3], size_t simdSize, const uint32_t workDim) {
    for (int i = 0; i < 3; i++)
        workGroupSize[i] = 1;
    size_t itemsPowerOfTwoDivisors[3] = {1, 1, 1};
    for (auto i = 0u; i < workDim; i++) {
        uint32_t requiredWorkItemsCount = maxWorkGroupSize;
        while (requiredWorkItemsCount > 1 && !(Math::isDivisableByPowerOfTwoDivisor(uint32_t(workItems[i]), requiredWorkItemsCount)))
            requiredWorkItemsCount = requiredWorkItemsCount >> 1;
        itemsPowerOfTwoDivisors[i] = requiredWorkItemsCount;
    }
    if (itemsPowerOfTwoDivisors[0] * itemsPowerOfTwoDivisors[1] >= maxWorkGroupSize) {
        while (itemsPowerOfTwoDivisors[0] * itemsPowerOfTwoDivisors[1] > maxWorkGroupSize) {
            if (itemsPowerOfTwoDivisors[0] > itemsPowerOfTwoDivisors[1])
                itemsPowerOfTwoDivisors[0] = itemsPowerOfTwoDivisors[0] >> 1;
            else
                itemsPowerOfTwoDivisors[1] = itemsPowerOfTwoDivisors[1] >> 1;
        }
        for (auto i = 0u; i < 3; i++)
            workGroupSize[i] = itemsPowerOfTwoDivisors[i];
        return;

    } else if (workItems[0] * workItems[1] > maxWorkGroupSize) {
        computeWorkgroupSize2D(maxWorkGroupSize, workGroupSize, workItems, simdSize);
        return;
    } else {
        for (auto i = 0u; i < workDim; i++)
            workGroupSize[i] = workItems[i];
        return;
    }
}

void computeWorkgroupSizeND(WorkSizeInfo wsInfo, size_t workGroupSize[3], const size_t workItems[3], const uint32_t workDim) {
    for (int i = 0; i < 3; i++)
        workGroupSize[i] = 1;

    uint64_t totalNuberOfItems = workItems[0] * workItems[1] * workItems[2];

    UNRECOVERABLE_IF(wsInfo.simdSize == 0);

    //Find biggest power of two which devide each dimension size
    if (wsInfo.slmTotalSize == 0 && !wsInfo.hasBarriers) {
        if (DebugManager.flags.EnableComputeWorkSizeSquared.get() && workDim == 2 && !wsInfo.imgUsed) {
            return computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workItems, wsInfo.simdSize, workDim);
        }

        size_t itemsPowerOfTwoDivisors[3] = {1, 1, 1};
        for (auto i = 0u; i < workDim; i++) {
            uint32_t requiredWorkItemsCount = uint32_t(wsInfo.simdSize * optimalHardwareThreadCountGeneric[0]);
            while (requiredWorkItemsCount > 1 && !(Math::isDivisableByPowerOfTwoDivisor(uint32_t(workItems[i]), requiredWorkItemsCount)))
                requiredWorkItemsCount = requiredWorkItemsCount >> 1;
            itemsPowerOfTwoDivisors[i] = requiredWorkItemsCount;
        }

        bool canUseNx4 = (wsInfo.imgUsed &&
                          (itemsPowerOfTwoDivisors[0] >= 4 || (itemsPowerOfTwoDivisors[0] >= 2 && wsInfo.simdSize == 8)) &&
                          itemsPowerOfTwoDivisors[1] >= 4);

        //If computed dimension sizes which are powers of two are creating group which is
        //bigger than maxWorkGroupSize or this group would create more than optimal hardware threads then downsize it
        uint64_t allItems = itemsPowerOfTwoDivisors[0] * itemsPowerOfTwoDivisors[1] * itemsPowerOfTwoDivisors[2];
        if (allItems > wsInfo.simdSize && (allItems > wsInfo.maxWorkGroupSize || allItems > wsInfo.simdSize * optimalHardwareThreadCountGeneric[0])) {
            computePowerOfTwoLWS(itemsPowerOfTwoDivisors, wsInfo, workGroupSize, workDim, canUseNx4);
            return;
        }
        //If coputed workgroup is at this point in correct size
        else if (allItems >= wsInfo.simdSize) {
            itemsPowerOfTwoDivisors[1] = canUseNx4 ? 4 : itemsPowerOfTwoDivisors[1];
            for (auto i = 0u; i < workDim; i++)
                workGroupSize[i] = itemsPowerOfTwoDivisors[i];
            return;
        }
    }
    //If dimensions are not powers of two but total number of items is less than max work group size
    if (totalNuberOfItems <= wsInfo.maxWorkGroupSize) {
        for (auto i = 0u; i < workDim; i++)
            workGroupSize[i] = workItems[i];
        return;
    } else {
        if (workDim == 1)
            computeWorkgroupSize1D(wsInfo.maxWorkGroupSize, workGroupSize, workItems, wsInfo.simdSize);
        else {
            uint32_t xyzFactors[3][1024];
            uint32_t xyzFactorsLen[3] = {};

            //check if algorithm should use ratio
            wsInfo.checkRatio(workItems);

            //find all divisors for all dimensions
            for (int i = 0; i < 3; i++)
                xyzFactors[i][xyzFactorsLen[i]++] = 1;
            for (auto i = 0u; i < workDim; i++) {
                for (auto j = 2u; j < wsInfo.maxWorkGroupSize; ++j) {
                    if ((workItems[i] % j) == 0) {
                        xyzFactors[i][xyzFactorsLen[i]++] = j;
                    }
                }
            }
            if (wsInfo.useRatio) {
                choosePreferredWorkGroupSizeWithRatio(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo);
                if (wsInfo.useStrictRatio && workGroupSize[0] * workGroupSize[1] * 2 <= wsInfo.simdSize) {
                    wsInfo.useStrictRatio = false;
                    choosePreferredWorkGroupSizeWithRatio(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo);
                }
            } else
                choosePreferredWorkGroupSizeWithOutRatio(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo, workDim);
        }
    }
}

Vec3<size_t> computeWorkgroupSize(const DispatchInfo &dispatchInfo) {
    size_t workGroupSize[3] = {};
    if (dispatchInfo.getKernel() != nullptr) {
        if (DebugManager.flags.EnableComputeWorkSizeND.get()) {
            WorkSizeInfo wsInfo(dispatchInfo);
            size_t workItems[3] = {dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z};
            computeWorkgroupSizeND(wsInfo, workGroupSize, workItems, dispatchInfo.getDim());
        } else {
            auto maxWorkGroupSize = static_cast<uint32_t>(dispatchInfo.getKernel()->getDevice().getDeviceInfo().maxWorkGroupSize);
            auto simd = dispatchInfo.getKernel()->getKernelInfo().getMaxSimdSize();
            size_t workItems[3] = {dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z};
            if (dispatchInfo.getDim() == 1) {
                computeWorkgroupSize1D(maxWorkGroupSize, workGroupSize, workItems, simd);
            } else if (DebugManager.flags.EnableComputeWorkSizeSquared.get() && dispatchInfo.getDim() == 2) {
                computeWorkgroupSizeSquared(maxWorkGroupSize, workGroupSize, workItems, simd, dispatchInfo.getDim());
            } else {
                computeWorkgroupSize2D(maxWorkGroupSize, workGroupSize, workItems, simd);
            }
        }
    }
    DBG_LOG(PrintLWSSizes, "Input GWS enqueueBlocked", dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z,
            " Driver deduced LWS", workGroupSize[0], workGroupSize[1], workGroupSize[2]);
    return {workGroupSize[0], workGroupSize[1], workGroupSize[2]};
}

Vec3<size_t> generateWorkgroupSize(const DispatchInfo &dispatchInfo) {
    return (dispatchInfo.getEnqueuedWorkgroupSize().x == 0) ? computeWorkgroupSize(dispatchInfo) : dispatchInfo.getEnqueuedWorkgroupSize();
}

Vec3<size_t> computeWorkgroupsNumber(const Vec3<size_t> gws, const Vec3<size_t> lws) {
    return (Vec3<size_t>(gws.x / lws.x + ((gws.x % lws.x) ? 1 : 0),
                         gws.y / lws.y + ((gws.y % lws.y) ? 1 : 0),
                         gws.z / lws.z + ((gws.z % lws.z) ? 1 : 0)));
}

Vec3<size_t> generateWorkgroupsNumber(const Vec3<size_t> gws, const Vec3<size_t> lws) {
    return (lws.x > 0) ? computeWorkgroupsNumber(gws, lws) : Vec3<size_t>(0, 0, 0);
}

Vec3<size_t> generateWorkgroupsNumber(const DispatchInfo &dispatchInfo) {
    return generateWorkgroupsNumber(dispatchInfo.getGWS(), dispatchInfo.getLocalWorkgroupSize());
}

Vec3<size_t> canonizeWorkgroup(Vec3<size_t> workgroup) {
    return ((workgroup.x > 0) ? Vec3<size_t>({workgroup.x, std::max(workgroup.y, static_cast<size_t>(1)), std::max(workgroup.z, static_cast<size_t>(1))})
                              : Vec3<size_t>(0, 0, 0));
}

void provideLocalWorkGroupSizeHints(Context *context, uint32_t maxWorkGroupSize, DispatchInfo dispatchInfo) {
    if (context != nullptr && context->isProvidingPerformanceHints() && dispatchInfo.getDim() <= 3) {
        size_t preferredWorkGroupSize[3];

        auto lws = computeWorkgroupSize(dispatchInfo);
        preferredWorkGroupSize[0] = lws.x;
        preferredWorkGroupSize[1] = lws.y;
        preferredWorkGroupSize[2] = lws.z;

        if (dispatchInfo.getEnqueuedWorkgroupSize().x == 0) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, NULL_LOCAL_WORKGROUP_SIZE, dispatchInfo.getKernel()->getKernelInfo().name.c_str(),
                                            preferredWorkGroupSize[0], preferredWorkGroupSize[1], preferredWorkGroupSize[2]);
        } else {
            size_t localWorkSizesIn[3] = {dispatchInfo.getEnqueuedWorkgroupSize().x, dispatchInfo.getEnqueuedWorkgroupSize().y, dispatchInfo.getEnqueuedWorkgroupSize().z};
            for (auto i = 0u; i < dispatchInfo.getDim(); i++) {
                if (localWorkSizesIn[i] != preferredWorkGroupSize[i]) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, BAD_LOCAL_WORKGROUP_SIZE,
                                                    localWorkSizesIn[0], localWorkSizesIn[1], localWorkSizesIn[2],
                                                    dispatchInfo.getKernel()->getKernelInfo().name.c_str(),
                                                    preferredWorkGroupSize[0], preferredWorkGroupSize[1], preferredWorkGroupSize[2]);
                    break;
                }
            }
        }
    }
}
} // namespace OCLRT
