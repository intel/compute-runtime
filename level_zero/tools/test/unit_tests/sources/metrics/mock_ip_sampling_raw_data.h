/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {
namespace ult {

class MockRawDataHelper {
  public:
    MockRawDataHelper() = default;
    ~MockRawDataHelper() = default;

    static constexpr uint32_t ipShift = 29;
    static constexpr uint32_t ipMask = 0x1fffffff;
    static constexpr uint32_t byteShift = 8;
    static constexpr uint32_t byteMask = 0xff;
    static constexpr uint32_t wordShift = 16;
    static constexpr uint32_t wordMask = 0xffff;

    struct RawReportElements {
        uint64_t ip;
        uint64_t activeCount;
        uint64_t otherCount;
        uint64_t controlCount;
        uint64_t pipeStallCount;
        uint64_t sendCount;
        uint64_t distAccCount;
        uint64_t sbidCount;
        uint64_t syncCount;
        uint64_t instFetchCount;
        uint64_t subSlice;
        uint64_t flags;
    };

    static void rawElementsToRawReports(uint32_t reportCount, std::vector<RawReportElements> inputElements, std::vector<std::array<uint64_t, 8>> &rawReports) {

        for (uint32_t i = 0; i < reportCount; i++) {
            rawReports[i][0] = (inputElements[i].ip & ipMask) |
                               ((inputElements[i].activeCount & byteMask) << ipShift) |
                               ((inputElements[i].otherCount & byteMask) << (ipShift + byteShift)) |
                               ((inputElements[i].controlCount & byteMask) << (ipShift + 2 * byteShift)) |
                               ((inputElements[i].pipeStallCount & byteMask) << (ipShift + 3 * byteShift)) |
                               ((inputElements[i].sendCount & 0x7) << (ipShift + 4 * byteShift));
            rawReports[i][1] = ((inputElements[i].sendCount & 0xf8) >> 3) |
                               ((inputElements[i].distAccCount & byteMask) << 5) |
                               ((inputElements[i].sbidCount & byteMask) << (5 + byteShift)) |
                               ((inputElements[i].syncCount & byteMask) << (5 + 2 * byteShift)) |
                               ((inputElements[i].instFetchCount & byteMask) << (5 + 3 * byteShift));
            rawReports[i][2] = 0LL;
            rawReports[i][3] = 0LL;
            rawReports[i][4] = 0LL;
            rawReports[i][5] = 0LL;
            rawReports[i][6] = (inputElements[i].subSlice & wordMask) | ((inputElements[i].flags & wordMask) << wordShift);
            rawReports[i][7] = 0;
        }
    }

    static void rawReportsToRawElements(uint32_t reportCount, const std::vector<std::array<uint64_t, 8>> &rawReports, std::vector<RawReportElements> &outputElements) {
        outputElements.resize(reportCount);
        for (uint32_t i = 0; i < reportCount; i++) {
            outputElements[i].ip = rawReports[i][0] & ipMask;
            outputElements[i].activeCount = (rawReports[i][0] >> ipShift) & byteMask;
            outputElements[i].otherCount = (rawReports[i][0] >> (ipShift + byteShift)) & byteMask;
            outputElements[i].controlCount = (rawReports[i][0] >> (ipShift + 2 * byteShift)) & byteMask;
            outputElements[i].pipeStallCount = (rawReports[i][0] >> (ipShift + 3 * byteShift)) & byteMask;
            outputElements[i].sendCount = ((rawReports[i][0] >> (ipShift + 4 * byteShift)) & 0x7) |
                                          ((rawReports[i][1] & 0x1f) << 3);
            outputElements[i].distAccCount = (rawReports[i][1] >> 5) & byteMask;
            outputElements[i].sbidCount = (rawReports[i][1] >> (5 + byteShift)) & byteMask;
            outputElements[i].syncCount = (rawReports[i][1] >> (5 + 2 * byteShift)) & byteMask;
            outputElements[i].instFetchCount = (rawReports[i][1] >> (5 + 3 * byteShift)) & byteMask;
            outputElements[i].subSlice = rawReports[i][6] & wordMask;
            outputElements[i].flags = (rawReports[i][6] >> wordShift) & wordMask;
        }
    }

    static void addMultiSubDevHeader(uint8_t *rawDataOut, size_t rawDataOutSize, uint8_t *rawDataIn, size_t rawDataInSize, uint32_t setIndex) {

        const auto expectedOutSize = sizeof(IpSamplingMultiDevDataHeader) + rawDataInSize;
        if (expectedOutSize <= rawDataOutSize) {

            auto header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(rawDataOut);
            header->magic = IpSamplingMultiDevDataHeader::magicValue;
            header->rawDataSize = static_cast<uint32_t>(rawDataInSize);
            header->setIndex = setIndex;
            memcpy_s(rawDataOut + sizeof(IpSamplingMultiDevDataHeader), rawDataOutSize - sizeof(IpSamplingMultiDevDataHeader),
                     rawDataIn, rawDataInSize);
        }
    }
};

} // namespace ult
} // namespace L0
