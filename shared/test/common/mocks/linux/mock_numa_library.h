/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/numa_library.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include <array>
#include <string>
#include <vector>

namespace NEO {

class MockNumaLibrary : public Linux::NumaLibrary {
  public:
    using NumaLibrary::mpolBind;
    using NumaLibrary::mpolDefault;
    using NumaLibrary::mpolFNumaBalancing;
    using NumaLibrary::mpolFRelativeNodes;
    using NumaLibrary::mpolFStaticNodes;
    using NumaLibrary::mpolLocal;
    using NumaLibrary::mpolPreferred;
    using NumaLibrary::mpolPreferredMany;
    using NumaLibrary::numaBitmaskFreeFunction;
    using NumaLibrary::numaGetRunNodeMaskFunction;
    using NumaLibrary::numaLoaded;

    static constexpr int mpolInterleave = 3;
    static constexpr uint32_t maxMockNodes = 4;

    explicit MockNumaLibrary(bool withDistance) {
        getMemPolicyRetVal = 0;
        policyMode = mpolDefault;
        policyNodeMask = {};
        runNodeMask = {};
        getRunNodeMaskFails = false;
        runNodeMaskFreeCalls = 0;
        for (uint32_t fromNode = 0; fromNode < maxMockNodes; fromNode++) {
            for (uint32_t toNode = 0; toNode < maxMockNodes; toNode++) {
                distances[fromNode][toNode] = (fromNode == toNode) ? localDistance : remoteDistance;
            }
        }

        MockOsLibrary::loadLibraryNewObject = createOsLibrary(withDistance);
        VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, MockOsLibraryCustom::load};
        NumaLibrary::init();
    }

    ~MockNumaLibrary() {
        osLibrary.reset();
        numaLoaded = false;
        getMemPolicyFunction = nullptr;
        numaAvailableFunction = nullptr;
        numaMaxNodeFunction = nullptr;
        numaDistanceFunction = nullptr;
        numaNumPossibleNodesFunction = nullptr;
        numaGetRunNodeMaskFunction = nullptr;
        numaBitmaskFreeFunction = nullptr;
        maxNode = -1;
        possibleNodes = 0;
    }

    static MockOsLibraryCustom *createOsLibrary(bool withDistance) {
        auto mockOsLibrary = new MockOsLibraryCustom(nullptr, true);
        mockOsLibrary->procMap[std::string(procGetMemPolicyStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockGetMemPolicy);
        mockOsLibrary->procMap[std::string(procNumaAvailableStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockNumaAvailable);
        mockOsLibrary->procMap[std::string(procNumaMaxNodeStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockNumaMaxNode);
        mockOsLibrary->procMap[std::string(procNumaNumPossibleNodesStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockNumaNumPossibleNodes);
        mockOsLibrary->procMap[std::string(procNumaGetRunNodeMaskStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockNumaGetRunNodeMask);
        mockOsLibrary->procMap[std::string(procNumaBitmaskFreeStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockNumaBitmaskFree);
        if (withDistance) {
            mockOsLibrary->procMap[std::string(procNumaDistanceStr)] = reinterpret_cast<void *>(&MockNumaLibrary::mockNumaDistance);
        }
        return mockOsLibrary;
    }

    static std::array<unsigned long, 2> createNodeMask(const std::vector<uint32_t> &nodes) {
        std::array<unsigned long, 2> nodeMask{};
        for (auto node : nodes) {
            nodeMask[node / bitsPerNodeMaskElement] |= 1ul << (node % bitsPerNodeMaskElement);
        }
        return nodeMask;
    }

    void setPolicy(int mode, const std::vector<uint32_t> &nodes) {
        policyMode = mode;
        policyNodeMask = createNodeMask(nodes);
    }

    void setRunNodes(const std::vector<uint32_t> &nodes) {
        runNodeMask = createNodeMask(nodes);
    }

    static constexpr int localDistance = 10;
    static constexpr int remoteDistance = 21;
    static constexpr int mockMaxNode = static_cast<int>(bitsPerNodeMaskElement) + 1;
    static constexpr int mockPossibleNodes = (mockMaxNode + 1) * static_cast<int>(bitsPerNodeMaskElement) * 2;

    static inline long getMemPolicyRetVal = 0;
    static inline int policyMode = mpolDefault;
    static inline std::array<unsigned long, 2> policyNodeMask{};
    static inline std::array<std::array<int, maxMockNodes>, maxMockNodes> distances{};
    static inline std::array<unsigned long, 2> runNodeMask{};
    static inline bool getRunNodeMaskFails = false;
    static inline uint32_t runNodeMaskFreeCalls = 0;
    static inline NumaBitmask runNodeBitmask{};

  protected:
    static long mockGetMemPolicy(int *mode, unsigned long nodeMask[], unsigned long maxNodeArg, void *, unsigned long) {
        if (maxNodeArg < static_cast<unsigned long>(mockPossibleNodes)) {
            return -1;
        }
        *mode = policyMode;
        nodeMask[0] = policyNodeMask[0];
        nodeMask[1] = policyNodeMask[1];
        return getMemPolicyRetVal;
    }
    static int mockNumaAvailable() { return 0; }
    static int mockNumaMaxNode() { return mockMaxNode; }
    static int mockNumaNumPossibleNodes() { return mockPossibleNodes; }
    static NumaBitmask *mockNumaGetRunNodeMask() {
        if (getRunNodeMaskFails) {
            return nullptr;
        }
        runNodeBitmask.size = runNodeMask.size() * bitsPerNodeMaskElement;
        runNodeBitmask.maskp = runNodeMask.data();
        return &runNodeBitmask;
    }
    static void mockNumaBitmaskFree(NumaBitmask *bitmask) {
        if (bitmask == &runNodeBitmask) {
            runNodeMaskFreeCalls++;
        }
    }
    static int mockNumaDistance(int fromNode, int toNode) {
        return distances[fromNode][toNode];
    }
};

} // namespace NEO
