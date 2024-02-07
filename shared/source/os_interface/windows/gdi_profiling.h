/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <algorithm>
#include <limits>
#include <vector>

namespace NEO {

class GdiProfiler {

    struct StatisticsEntry {
        long long totalTime = 0;
        uint64_t count = 0;
        long long minTime = std::numeric_limits<long long>::max();
        long long maxTime = 0;
        const char *gdiCall = nullptr;
        size_t getLength() const {
            return this->gdiCall ? strlen(this->gdiCall) : 0u;
        }
    };

  public:
    void printGdiTimes() {
        if (this->gdiStatistics.empty()) {
            return;
        }

        auto maxCallLengthIt = std::max_element(this->gdiStatistics.begin(), this->gdiStatistics.end(), [](const auto &gdiData1, const auto &gdiData2) {
            return gdiData1.getLength() < gdiData2.getLength();
        });
        auto maxCallLength = static_cast<int>(strlen(maxCallLengthIt->gdiCall));

        printf("\n--- Gdi statistics ---\n");
        printf("%*s %15s %10s %25s %15s %15s", maxCallLength, "Request", "Total time(ns)", "Count", "Avg time per gdi call", "Min", "Max\n");
        for (const auto &gdiData : this->gdiStatistics) {
            if (gdiData.count == 0) {
                continue;
            }
            printf("%*s %15llu %10lu %25f %15lld %15lld\n",
                   maxCallLength,
                   gdiData.gdiCall,
                   gdiData.totalTime,
                   static_cast<unsigned long>(gdiData.count),
                   gdiData.totalTime / static_cast<double>(gdiData.count),
                   gdiData.minTime,
                   gdiData.maxTime);
        }
        printf("\n");
    }

    void recordElapsedTime(long long elapsedTime, const char *name, uint32_t id) {
        if (this->gdiStatistics.size() <= id) {
            this->gdiStatistics.resize(id + 1u);
        }

        auto &gdiData = this->gdiStatistics[id];

        gdiData.gdiCall = name;
        gdiData.totalTime += elapsedTime;
        gdiData.count++;
        gdiData.minTime = std::min(gdiData.minTime, elapsedTime);
        gdiData.maxTime = std::max(gdiData.maxTime, elapsedTime);
    }

  protected:
    std::vector<StatisticsEntry> gdiStatistics{};
};
} // namespace NEO