/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_work_size.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/work_size_info.h"

#include <cmath>
#include <cstdint>

namespace NEO {

// threshold used to determine what kind of device is underneath
// big cores like SKL have 8EU * 7 HW threads per subslice and are considered as highThreadCount devices
constexpr uint32_t highThreadCountThreshold = 56u;

constexpr uint32_t optimalHardwareThreadCountGeneric[] = {32, 16, 8, 4, 2, 1};

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

static const size_t maxPrimes = sizeof(primeNumbers) / sizeof(primeNumbers[0]);

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
    auto simdSize = workGroupInfo.simdSize;

    while (optimalHardwareThreadCountGeneric[targetIndex] > 1 &&
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

void choosePreferredWorkGroupSizeWithRatio(uint32_t xyzFactors[3][1024], uint32_t xyzFactorsLen[3], size_t workGroupSize[3], const size_t workItems[3], WorkSizeInfo &wsInfo, bool enforceDescendingOrder) {
    float localRatio = std::numeric_limits<float>::max();
    uint64_t localNumWorkgroups = std::numeric_limits<uint64_t>::max();
    for (uint32_t xFactorsIdx = 0; xFactorsIdx < xyzFactorsLen[0]; ++xFactorsIdx) {
        for (uint32_t yFactorsIdx = 0; yFactorsIdx < xyzFactorsLen[1]; ++yFactorsIdx) {

            uint32_t xdim = xyzFactors[0][xyzFactorsLen[0] - 1 - xFactorsIdx];
            uint32_t ydim = xyzFactors[1][yFactorsIdx];

            if (enforceDescendingOrder && ydim > xdim) {
                break;
            }

            if ((xdim * ydim) > wsInfo.maxWorkGroupSize) {
                break;
            }
            if ((xdim * ydim) < wsInfo.minWorkGroupSize) {
                continue;
            }

            uint64_t numWorkGroups = Math::divideAndRoundUp(workItems[0], xdim);
            numWorkGroups *= Math::divideAndRoundUp(workItems[1], ydim);

            float ratioDiff = log(static_cast<float>(xdim)) - log(static_cast<float>(ydim));
            ratioDiff = fabs(wsInfo.targetRatio - ratioDiff);

            bool setWorkGroupSize = wsInfo.useStrictRatio
                                        ? (ratioDiff < localRatio)
                                        : (numWorkGroups < localNumWorkgroups) || ((numWorkGroups == localNumWorkgroups) && (ratioDiff < localRatio));
            if (setWorkGroupSize) {
                workGroupSize[0] = xdim;
                workGroupSize[1] = ydim;
                localRatio = ratioDiff;
                localNumWorkgroups = numWorkGroups;
            }
        }
    }
}

void choosePreferredWorkGroupSizeWithOutRatio(uint32_t xyzFactors[3][1024], uint32_t xyzFactorsLen[3], size_t workGroupSize[3], const size_t workItems[3], WorkSizeInfo &wsInfo, bool enforceDescendingOrder) {
    uint64_t localEuThrdsDispatched = std::numeric_limits<uint64_t>::max();

    for (uint32_t xFactorsIdx = 0; xFactorsIdx < xyzFactorsLen[0]; ++xFactorsIdx) {
        for (uint32_t yFactorsIdx = 0; yFactorsIdx < xyzFactorsLen[1]; ++yFactorsIdx) {
            for (uint32_t zFactorsIdx = 0; zFactorsIdx < xyzFactorsLen[2]; ++zFactorsIdx) {

                uint32_t xdim = xyzFactors[0][xyzFactorsLen[0] - 1 - xFactorsIdx];
                uint32_t ydim = xyzFactors[1][xyzFactorsLen[1] - 1 - yFactorsIdx];
                uint32_t zdim = xyzFactors[2][xyzFactorsLen[2] - 1 - zFactorsIdx];

                if (enforceDescendingOrder) {
                    if (ydim > xdim) {
                        break;
                    } else if (zdim > ydim) {
                        continue;
                    }
                }

                uint32_t numItemsInWorkGroup = xdim * ydim * zdim;
                if (numItemsInWorkGroup > wsInfo.maxWorkGroupSize) {
                    continue;
                }
                if (numItemsInWorkGroup < wsInfo.minWorkGroupSize) {
                    break;
                }

                uint64_t numWorkGroups = Math::divideAndRoundUp(workItems[0], xdim);
                numWorkGroups *= Math::divideAndRoundUp(workItems[1], ydim);
                numWorkGroups *= Math::divideAndRoundUp(workItems[2], zdim);
                uint64_t numThreadsPerWorkGroup = Math::divideAndRoundUp(numItemsInWorkGroup, wsInfo.simdSize);
                uint64_t euThrdsDispatched = numThreadsPerWorkGroup * numWorkGroups;
                if (euThrdsDispatched < localEuThrdsDispatched) {
                    localEuThrdsDispatched = euThrdsDispatched;
                    workGroupSize[0] = xdim;
                    workGroupSize[1] = ydim;
                    workGroupSize[2] = zdim;
                }
            }
        }
    }
}

void computeWorkgroupSize1D(uint32_t maxWorkGroupSize, size_t workGroupSize[3], const size_t workItems[3], size_t simdSize) {
    auto items = workItems[0];

    // Determine the LSB set to quickly handle factors of 2
    auto numBits = Math::getMinLsbSet(static_cast<uint32_t>(items));

    // Clamp power of 2 result to maxWorkGroupSize
    uint32_t workSize = 1u << numBits;

    // Assumes maxWorkGroupSize is a power of two.
    DEBUG_BREAK_IF((maxWorkGroupSize & (maxWorkGroupSize - 1)) != 0);
    workSize = std::min(workSize, maxWorkGroupSize);

    // Try all primes as potential factors
    workSize = factor<maxPrimes - 1>(items, workSize, maxWorkGroupSize);

    workGroupSize[0] = workSize;
    workGroupSize[1] = 1;
    workGroupSize[2] = 1;
}

void choosePreferredWorkgroupSize(uint32_t xyzFactors[3][1024], uint32_t xyzFactorsLen[3], size_t workGroupSize[3], const size_t workItems[3], WorkSizeInfo &wsInfo, bool enforceDescendingOrder) {
    // check if algorithm should use ratio
    wsInfo.checkRatio(workItems);

    if (wsInfo.useRatio) {
        choosePreferredWorkGroupSizeWithRatio(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo, enforceDescendingOrder);
        if (wsInfo.useStrictRatio && workGroupSize[0] * workGroupSize[1] * 2 <= wsInfo.simdSize) {
            wsInfo.useStrictRatio = false;
            choosePreferredWorkGroupSizeWithRatio(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo, enforceDescendingOrder);
        }
    } else {
        choosePreferredWorkGroupSizeWithOutRatio(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo, enforceDescendingOrder);
    }
}

void choosePrefferedWorkgroupSize(WorkSizeInfo &wsInfo, size_t workGroupSize[3], const size_t workItems[3], const uint32_t workDim) {
    // find all divisors for all dimensions
    uint32_t xyzFactors[3][1024];
    uint32_t xyzFactorsLen[3] = {};
    for (int i = 0; i < 3; i++)
        xyzFactors[i][xyzFactorsLen[i]++] = 1;
    for (auto i = 0u; i < workDim; i++) {
        for (auto j = 2u; j < wsInfo.maxWorkGroupSize; ++j) {
            if ((workItems[i] % j) == 0) {
                xyzFactors[i][xyzFactorsLen[i]++] = j;
            }
        }
    }

    choosePreferredWorkgroupSize(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo, true);
    size_t wgs = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
    if (wgs * 2 <= wsInfo.simdSize) {
        choosePreferredWorkgroupSize(xyzFactors, xyzFactorsLen, workGroupSize, workItems, wsInfo, false);
    }
}

void computeWorkgroupSize2D(uint32_t maxWorkGroupSize, size_t workGroupSize[3], const size_t workItems[3], size_t simdSize) {
    uint32_t xFactors[1024];
    uint32_t yFactors[1024];
    uint32_t xFactorsLen = 0;
    uint32_t yFactorsLen = 0;
    uint64_t waste;
    uint64_t localWSWaste = 0xffffffffffffffff;
    uint64_t euThrdsDispatched;
    uint64_t localEuThrdsDispatched = 0xffffffffffffffff;
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
            workGroups = Math::divideAndRoundUp(workItems[0], xDim);
            workGroups *= Math::divideAndRoundUp(workItems[1], yDim);

            // Compaction Mode!
            euThrdsDispatched = Math::divideAndRoundUp(xDim * yDim, simdSize);
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
        while (requiredWorkItemsCount > 1 && !(Math::isDivisibleByPowerOfTwoDivisor(uint32_t(workItems[i]), requiredWorkItemsCount)))
            requiredWorkItemsCount >>= 1;
        itemsPowerOfTwoDivisors[i] = requiredWorkItemsCount;
    }
    if (itemsPowerOfTwoDivisors[0] * itemsPowerOfTwoDivisors[1] >= maxWorkGroupSize) {
        while (itemsPowerOfTwoDivisors[0] * itemsPowerOfTwoDivisors[1] > maxWorkGroupSize) {
            if (itemsPowerOfTwoDivisors[0] > itemsPowerOfTwoDivisors[1])
                itemsPowerOfTwoDivisors[0] >>= 1;
            else
                itemsPowerOfTwoDivisors[1] >>= 1;
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

void computeWorkgroupSizeND(WorkSizeInfo &wsInfo, size_t workGroupSize[3], const size_t workItems[3], const uint32_t workDim) {
    for (int i = 0; i < 3; i++)
        workGroupSize[i] = 1;

    UNRECOVERABLE_IF(wsInfo.simdSize == 0);
    uint64_t totalNumberOfItems = workItems[0] * workItems[1] * workItems[2];
    auto optimalWgThreadCount = optimalHardwareThreadCountGeneric[0];
    bool totalRequiredThreadGroupsMoreThanSingleThreadGroup = totalNumberOfItems > wsInfo.simdSize * optimalWgThreadCount;

    // Find biggest power of two which divide each dimension size
    if (wsInfo.slmTotalSize == 0 && !wsInfo.hasBarriers) {
        if (debugManager.flags.EnableComputeWorkSizeSquared.get() && workDim == 2 && !wsInfo.imgUsed) {
            return computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workItems, wsInfo.simdSize, workDim);
        }

        if (wsInfo.preferredWgCountPerSubSlice != 0 && wsInfo.simdSize == 32 && totalRequiredThreadGroupsMoreThanSingleThreadGroup) {
            optimalWgThreadCount = std::min(optimalWgThreadCount, wsInfo.numThreadsPerSubSlice / wsInfo.preferredWgCountPerSubSlice);
            wsInfo.maxWorkGroupSize = wsInfo.simdSize * optimalWgThreadCount;
        }

        size_t itemsPowerOfTwoDivisors[3] = {1, 1, 1};
        for (auto i = 0u; i < workDim; i++) {
            uint32_t requiredWorkItemsCount = uint32_t(wsInfo.simdSize * optimalWgThreadCount);
            while (requiredWorkItemsCount > 1 && !(Math::isDivisibleByPowerOfTwoDivisor(uint32_t(workItems[i]), requiredWorkItemsCount)))
                requiredWorkItemsCount >>= 1;
            itemsPowerOfTwoDivisors[i] = requiredWorkItemsCount;
        }

        bool canUseNx4 = (wsInfo.imgUsed &&
                          (itemsPowerOfTwoDivisors[0] >= 4 || (itemsPowerOfTwoDivisors[0] >= 2 && wsInfo.simdSize == 8)) &&
                          itemsPowerOfTwoDivisors[1] >= 4);

        // If computed dimension sizes which are powers of two are creating group which is
        // bigger than maxWorkGroupSize or this group would create more than optimal hardware threads then downsize it
        uint64_t allItems = itemsPowerOfTwoDivisors[0] * itemsPowerOfTwoDivisors[1] * itemsPowerOfTwoDivisors[2];
        if (allItems > wsInfo.simdSize && (allItems > wsInfo.maxWorkGroupSize || allItems > wsInfo.simdSize * optimalWgThreadCount)) {
            return computePowerOfTwoLWS(itemsPowerOfTwoDivisors, wsInfo, workGroupSize, workDim, canUseNx4);
        }
        // If computed workgroup is at this point in correct size
        else if (allItems >= wsInfo.simdSize) {
            itemsPowerOfTwoDivisors[1] = canUseNx4 ? 4 : itemsPowerOfTwoDivisors[1];
            for (auto i = 0u; i < workDim; i++)
                workGroupSize[i] = itemsPowerOfTwoDivisors[i];
            return;
        }
    }

    // If dimensions are not powers of two but total number of items is less than max work group size
    if (totalNumberOfItems <= wsInfo.maxWorkGroupSize) {
        for (auto i = 0u; i < workDim; i++)
            workGroupSize[i] = workItems[i];
        return;
    }

    if (workDim == 1) {
        return computeWorkgroupSize1D(wsInfo.maxWorkGroupSize, workGroupSize, workItems, wsInfo.simdSize);
    }

    choosePrefferedWorkgroupSize(wsInfo, workGroupSize, workItems, workDim);
}

Vec3<size_t> computeWorkgroupsNumber(const Vec3<size_t> &gws, const Vec3<size_t> &lws) {
    return (Vec3<size_t>(gws.x / lws.x + ((gws.x % lws.x) ? 1 : 0),
                         gws.y / lws.y + ((gws.y % lws.y) ? 1 : 0),
                         gws.z / lws.z + ((gws.z % lws.z) ? 1 : 0)));
}

Vec3<size_t> generateWorkgroupsNumber(const Vec3<size_t> &gws, const Vec3<size_t> &lws) {
    return (lws.x > 0) ? computeWorkgroupsNumber(gws, lws) : Vec3<size_t>(0, 0, 0);
}

Vec3<size_t> canonizeWorkgroup(const Vec3<size_t> &workgroup) {
    return ((workgroup.x > 0) ? Vec3<size_t>({workgroup.x, std::max(workgroup.y, static_cast<size_t>(1)), std::max(workgroup.z, static_cast<size_t>(1))})
                              : Vec3<size_t>(0, 0, 0));
}

} // namespace NEO
