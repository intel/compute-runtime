/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/topology.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"

#include <bit>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace NEO {

TopologyInfo getTopologyInfo(HardwareInfo &hwInfo, const TopologyBitmap &topologyBitmap, const TopologyLimits &topologyLimits, TopologyMapping &topologyMapping, bool scanFullBitmap) {
    TopologyInfo topologyInfo{};

    std::vector<int> sliceIndices;
    sliceIndices.reserve(topologyLimits.maxSlices);

    std::vector<int> subSliceIndices;
    subSliceIndices.reserve(topologyLimits.maxSubSlicesPerSlice);

    auto processSubSlices = [&](std::span<const uint8_t> subSliceBitmap) -> std::tuple<int, int, int> {
        int regionCount = 0;
        int sliceCount = 0;
        int subSliceCount = 0;

        if (!scanFullBitmap) {
            UNRECOVERABLE_IF(topologyLimits.maxRegions == 0);
            DEBUG_BREAK_IF(topologyLimits.maxSlices % topologyLimits.maxRegions != 0);
            const auto maxSlicesPerRegion = topologyLimits.maxSlices / topologyLimits.maxRegions;

            for (auto regionId = 0; regionId < topologyLimits.maxRegions; ++regionId) {
                auto sliceCountPerRegion = 0;

                for (auto sliceIdPerRegion = 0; sliceIdPerRegion < maxSlicesPerRegion; ++sliceIdPerRegion) {
                    int sliceId = regionId * maxSlicesPerRegion + sliceIdPerRegion;
                    int subSliceCountPerSlice = 0;

                    for (auto subSliceIdPerSlice = 0; subSliceIdPerSlice < topologyLimits.maxSubSlicesPerSlice; ++subSliceIdPerSlice) {
                        const auto subSliceId = sliceId * topologyLimits.maxSubSlicesPerSlice + subSliceIdPerSlice;
                        const auto byte = subSliceId / 8u;
                        const auto bit = subSliceId % 8u;

                        if (subSliceId >= std::ssize(subSliceBitmap) * 8) {
                            break;
                        }

                        if (subSliceBitmap[byte] & (1u << bit)) {
                            subSliceIndices.push_back(subSliceIdPerSlice);
                            subSliceCountPerSlice += 1;
                            if (sliceId < GT_MAX_SLICE && subSliceIdPerSlice < GT_MAX_SUBSLICE_PER_SLICE) {
                                hwInfo.gtSystemInfo.SliceInfo[sliceId].SubSliceInfo[subSliceIdPerSlice].Enabled = true;
                            }
                        }
                    }

                    if (subSliceCountPerSlice) {
                        sliceIndices.push_back(sliceId);
                        sliceCountPerRegion += 1;
                        subSliceCount += subSliceCountPerSlice;
                        if (sliceId < GT_MAX_SLICE) {
                            hwInfo.gtSystemInfo.SliceInfo[sliceId].Enabled = true;
                        }

                        if (sliceCountPerRegion == 1) {
                            topologyMapping.subsliceIndices = std::move(subSliceIndices);
                        }
                    }

                    subSliceIndices.clear();
                }

                if (sliceCountPerRegion) {
                    regionCount += 1;
                    sliceCount += sliceCountPerRegion;
                }
            }
        } else {
            regionCount = topologyLimits.maxRegions;

            for (int byteIdx = 0; byteIdx < std::ssize(subSliceBitmap); ++byteIdx) {
                for (int bitIdx = 0; bitIdx < 8; ++bitIdx) {
                    if (subSliceBitmap[byteIdx] & (1u << bitIdx)) {
                        subSliceIndices.push_back(subSliceCount++);
                    }
                }
            }

            sliceCount = static_cast<int>(Math::divideAndRoundUp(static_cast<size_t>(subSliceCount), static_cast<size_t>(topologyLimits.maxSubSlicesPerSlice)));

            for (int sliceId = 0; sliceId < sliceCount; ++sliceId) {
                sliceIndices.push_back(sliceId);
                hwInfo.gtSystemInfo.SliceInfo[sliceId].Enabled = true;
            }

            if (sliceCount == 1) {
                topologyMapping.subsliceIndices = std::move(subSliceIndices);
            }
        }

        return {regionCount, sliceCount, subSliceCount};
    };

    auto [regionCount, sliceCount, subSliceCount] = processSubSlices(topologyBitmap.dssCompute);

    if (!subSliceCount) {
        std::tie(regionCount, sliceCount, subSliceCount) = processSubSlices(topologyBitmap.dssGeometry);
    }

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    topologyMapping.sliceIndices = std::move(sliceIndices);
    if (sliceCount != 1) {
        topologyMapping.subsliceIndices.clear();
    }

    auto bitmapCount = [](std::span<const uint8_t> bitmap) {
        return std::transform_reduce(bitmap.begin(), bitmap.end(), 0, std::plus{}, std::popcount<uint8_t>);
    };

    topologyInfo.regionCount = regionCount;
    topologyInfo.sliceCount = sliceCount;
    topologyInfo.subSliceCount = subSliceCount;
    topologyInfo.euCount = bitmapCount(topologyBitmap.eu) * topologyInfo.subSliceCount;
    topologyInfo.l3BankCount = bitmapCount(topologyBitmap.l3Banks);

    return topologyInfo;
}

TopologyInfo getTopologyInfoMultiTile(HardwareInfo &hwInfo, std::span<const TopologyBitmap> topologyBitmap, const TopologyLimits &topologyLimits, TopologyMap &topologyMap, bool scanFullBitmap) {
    const auto numTiles = std::ssize(topologyBitmap);

    if (0 == numTiles) {
        return TopologyInfo{};
    }

    std::vector<TopologyInfo> topologyInfos;
    topologyInfos.reserve(numTiles);

    for (auto i = 0; i < numTiles; ++i) {
        topologyInfos.push_back(getTopologyInfo(hwInfo, topologyBitmap[i], topologyLimits, topologyMap[i], scanFullBitmap));
    }

    TopologyInfo topologyInfo{
        .regionCount = std::numeric_limits<decltype(TopologyInfo::regionCount)>::max(),
        .sliceCount = std::numeric_limits<decltype(TopologyInfo::sliceCount)>::max(),
        .subSliceCount = std::numeric_limits<decltype(TopologyInfo::subSliceCount)>::max(),
        .euCount = std::numeric_limits<decltype(TopologyInfo::euCount)>::max(),
        .l3BankCount = std::numeric_limits<decltype(TopologyInfo::l3BankCount)>::max(),
    };

    topologyInfo = std::reduce(topologyInfos.cbegin(), topologyInfos.cend(), topologyInfo, [](const TopologyInfo &topoInfo1, const TopologyInfo &topoInfo2) {
        return TopologyInfo{
            .regionCount = std::min(topoInfo1.regionCount, topoInfo2.regionCount),
            .sliceCount = std::min(topoInfo1.sliceCount, topoInfo2.sliceCount),
            .subSliceCount = std::min(topoInfo1.subSliceCount, topoInfo2.subSliceCount),
            .euCount = std::min(topoInfo1.euCount, topoInfo2.euCount),
            .l3BankCount = std::min(topoInfo1.l3BankCount, topoInfo2.l3BankCount),
        };
    });

    return topologyInfo;
}

} // namespace NEO
