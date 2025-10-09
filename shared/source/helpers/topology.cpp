/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/topology.h"

#include "shared/source/helpers/hw_info.h"

#include <bit>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace NEO {

TopologyInfo getTopologyInfo(HardwareInfo &hwInfo, const TopologyBitmap &topologyBitmap, const TopologyLimits &topologyLimits, TopologyMapping &topologyMapping) {
    TopologyInfo topologyInfo{};

    std::vector<int> sliceIndices;
    sliceIndices.reserve(topologyLimits.maxSlices);

    std::vector<int> subSliceIndices;
    subSliceIndices.reserve(topologyLimits.maxSubSlicesPerSlice);

    auto processSubSlices = [&](std::span<const uint8_t> subSliceBitmap) -> std::pair<int, int> {
        int sliceCount = 0;
        int subSliceCountTotal = 0;

        for (auto sliceId = 0; sliceId < topologyLimits.maxSlices; ++sliceId) {
            int subSliceCount = 0;

            for (auto subSliceId = 0; subSliceId < topologyLimits.maxSubSlicesPerSlice; ++subSliceId) {
                const auto idx = sliceId * topologyLimits.maxSubSlicesPerSlice + subSliceId;
                const auto byte = idx / 8u;
                const auto bit = idx % 8u;

                if (idx >= std::ssize(subSliceBitmap) * 8) {
                    break;
                }

                if (subSliceBitmap[byte] & (1u << bit)) {
                    subSliceIndices.push_back(subSliceId);
                    subSliceCount += 1;
                    if (sliceId < GT_MAX_SLICE && subSliceId < GT_MAX_SUBSLICE_PER_SLICE) {
                        hwInfo.gtSystemInfo.SliceInfo[sliceId].SubSliceInfo[subSliceId].Enabled = true;
                    }
                }
            }

            if (subSliceCount) {
                sliceIndices.push_back(sliceId);
                sliceCount += 1;
                subSliceCountTotal += subSliceCount;
                if (sliceId < GT_MAX_SLICE) {
                    hwInfo.gtSystemInfo.SliceInfo[sliceId].Enabled = true;
                }
            }

            if (sliceCount == 1) {
                topologyMapping.subsliceIndices = std::move(subSliceIndices);
            }

            subSliceIndices.clear();
        }

        return {sliceCount, subSliceCountTotal};
    };

    auto [sliceCount, subSliceCount] = processSubSlices(topologyBitmap.dssCompute);

    if (!subSliceCount) {
        std::tie(sliceCount, subSliceCount) = processSubSlices(topologyBitmap.dssGeometry);
    }

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    topologyMapping.sliceIndices = std::move(sliceIndices);
    if (sliceCount != 1) {
        topologyMapping.subsliceIndices.clear();
    }

    auto bitmapCount = [](std::span<const uint8_t> bitmap) {
        return std::transform_reduce(bitmap.begin(), bitmap.end(), 0, std::plus{}, std::popcount<uint8_t>);
    };

    topologyInfo.sliceCount = sliceCount;
    topologyInfo.subSliceCount = subSliceCount;
    topologyInfo.euCount = bitmapCount(topologyBitmap.eu) * topologyInfo.subSliceCount;
    topologyInfo.l3BankCount = bitmapCount(topologyBitmap.l3Banks);

    return topologyInfo;
}

TopologyInfo getTopologyInfoMultiTile(HardwareInfo &hwInfo, std::span<const TopologyBitmap> topologyBitmap, const TopologyLimits &topologyLimits, TopologyMap &topologyMap) {
    const auto numTiles = std::ssize(topologyBitmap);

    if (0 == numTiles) {
        return TopologyInfo{};
    }

    std::vector<TopologyInfo> topologyInfos;
    topologyInfos.reserve(numTiles);

    for (auto i = 0; i < numTiles; ++i) {
        topologyInfos.push_back(getTopologyInfo(hwInfo, topologyBitmap[i], topologyLimits, topologyMap[i]));
    }

    TopologyInfo topologyInfo{
        .sliceCount = std::numeric_limits<decltype(TopologyInfo::sliceCount)>::max(),
        .subSliceCount = std::numeric_limits<decltype(TopologyInfo::subSliceCount)>::max(),
        .euCount = std::numeric_limits<decltype(TopologyInfo::euCount)>::max(),
        .l3BankCount = std::numeric_limits<decltype(TopologyInfo::l3BankCount)>::max(),
    };

    topologyInfo = std::reduce(topologyInfos.cbegin(), topologyInfos.cend(), topologyInfo, [](const TopologyInfo &topoInfo1, const TopologyInfo &topoInfo2) {
        return TopologyInfo{
            .sliceCount = std::min(topoInfo1.sliceCount, topoInfo2.sliceCount),
            .subSliceCount = std::min(topoInfo1.subSliceCount, topoInfo2.subSliceCount),
            .euCount = std::min(topoInfo1.euCount, topoInfo2.euCount),
            .l3BankCount = std::min(topoInfo1.l3BankCount, topoInfo2.l3BankCount),
        };
    });

    return topologyInfo;
}

} // namespace NEO
