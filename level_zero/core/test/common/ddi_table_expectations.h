/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/ddi/ze_ddi_tables.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace L0 {
namespace ult {

struct DdiEntryExpectation {
    const char *name;
    size_t offset;
    ze_api_version_t exposedSinceVersion;
};

struct DdiTableExpectation {
    const char *name;
    size_t tableSize;
    ze_result_t (*getProcAddrTable)(ze_api_version_t version, void *ddiTable);
    const DdiEntryExpectation *entries;
    size_t entriesCount;
};

// Describes one entry of the DDI table aliased as Table in the enclosing scope.
#define DDI_ENTRY(member, exposedSinceVersion) \
    DdiEntryExpectation { #member, offsetof(Table, member), exposedSinceVersion }

// Binds a manifest namespace to the entry point that fills its DDI table.
#define DDI_TABLE(tableNamespace, getProcAddrTableFunc)                                               \
    DdiTableExpectation {                                                                             \
        #getProcAddrTableFunc, sizeof(tableNamespace::Table),                                         \
            [](ze_api_version_t version, void *ddiTable) {                                            \
                return getProcAddrTableFunc(version, static_cast<tableNamespace::Table *>(ddiTable)); \
            },                                                                                        \
            tableNamespace::entries, std::size(tableNamespace::entries)                               \
    }

using DdiTableStorage = std::vector<void *>;

inline size_t getDdiSlotsCount(const DdiTableExpectation &table) {
    return table.tableSize / sizeof(void *);
}

inline DdiTableStorage createDdiTableStorage(const DdiTableExpectation &table) {
    return DdiTableStorage(getDdiSlotsCount(table), nullptr);
}

inline const void *getDdiSlot(const DdiTableStorage &storage, size_t slot) {
    const void *slotValue = nullptr;
    memcpy_s(&slotValue, sizeof(slotValue), &storage[slot], sizeof(slotValue));
    return slotValue;
}

inline std::vector<ze_api_version_t> getAllApiVersions() {
    const auto latestMinor = static_cast<uint32_t>(ZE_MINOR_VERSION(ZE_API_VERSION_CURRENT));
    std::vector<ze_api_version_t> versions;
    for (uint32_t minor = 0; minor <= latestMinor; minor++) {
        versions.push_back(static_cast<ze_api_version_t>(ZE_MAKE_VERSION(1, minor)));
    }
    return versions;
}

inline std::string apiVersionToString(ze_api_version_t version) {
    if (ZE_API_VERSION_FORCE_UINT32 == version) {
        return "ZE_API_VERSION_FORCE_UINT32";
    }
    return "ZE_API_VERSION_" + std::to_string(ZE_MAJOR_VERSION(version)) + "_" + std::to_string(ZE_MINOR_VERSION(version));
}

inline std::string describeDdiEntry(const DdiTableExpectation &table, const DdiEntryExpectation &entry) {
    return std::string(table.name) + " / " + entry.name + " (exposed since " + apiVersionToString(entry.exposedSinceVersion) + ")";
}

inline std::string describeDdiSlot(const DdiTableExpectation &table, size_t slot) {
    return std::string(table.name) + " / slot " + std::to_string(slot) + " (not described by the manifest)";
}

inline void givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnceFunction(const DdiTableExpectation *tables, size_t tablesCount) {
    for (size_t t = 0; t < tablesCount; t++) {
        const auto &table = tables[t];
        std::vector<const char *> describedSlots(getDdiSlotsCount(table), nullptr);
        for (size_t i = 0; i < table.entriesCount; i++) {
            const auto &entry = table.entries[i];
            const size_t slot = entry.offset / sizeof(void *);
            EXPECT_EQ(nullptr, describedSlots[slot])
                << describeDdiEntry(table, entry) << " describes slot " << slot << " already described by " << describedSlots[slot];
            describedSlots[slot] = entry.name;
        }
    }
}

inline ze_api_version_t getHighestExposedApiVersion(const DdiTableExpectation *tables, size_t tablesCount, std::string &highestEntryName) {
    auto highest = ZE_API_VERSION_1_0;
    highestEntryName = "<no entry>";
    for (size_t t = 0; t < tablesCount; t++) {
        const auto &table = tables[t];
        for (size_t i = 0; i < table.entriesCount; i++) {
            const auto &entry = table.entries[i];
            if (entry.exposedSinceVersion > highest) {
                highest = entry.exposedSinceVersion;
                highestEntryName = std::string(table.name) + " / " + entry.name;
            }
        }
    }
    return highest;
}

inline void givenDdiTableManifestWhenComparingAgainstComponentVersionThenItMatchesHighestExposedApiVersionFunction(const DdiTableExpectation *tables, size_t tablesCount, ze_api_version_t componentVersion) {
    std::string highestEntryName;
    const auto highestExposed = getHighestExposedApiVersion(tables, tablesCount, highestEntryName);

    // As per DDI handles extension L0 loader reads a component's dispatch table only up to the version that table declares,
    // so an entry above the declared version is filled by the driver but never reached.
    EXPECT_EQ(highestExposed, componentVersion)
        << highestEntryName << " is exposed since " << apiVersionToString(highestExposed)
        << ", the dispatch table declares " << apiVersionToString(componentVersion);
}

inline void givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulatedFunction(const DdiTableExpectation *tables, size_t tablesCount) {
    for (const auto version : getAllApiVersions()) {
        for (size_t t = 0; t < tablesCount; t++) {
            const auto &table = tables[t];
            auto storage = createDdiTableStorage(table);

            EXPECT_EQ(ZE_RESULT_SUCCESS, table.getProcAddrTable(version, storage.data()))
                << table.name << " called with " << apiVersionToString(version);

            // Check the slot every manifest entry describes, then clear it. Whatever the driver
            // filled beyond the manifest stays behind and is reported by the sweep below.
            for (size_t i = 0; i < table.entriesCount; i++) {
                const auto &entry = table.entries[i];
                const size_t slot = entry.offset / sizeof(void *);
                if (version >= entry.exposedSinceVersion) {
                    EXPECT_NE(nullptr, getDdiSlot(storage, slot))
                        << describeDdiEntry(table, entry) << " is not exposed for " << apiVersionToString(version);
                } else {
                    EXPECT_EQ(nullptr, getDdiSlot(storage, slot))
                        << describeDdiEntry(table, entry) << " is unexpectedly exposed for " << apiVersionToString(version);
                }
                storage[slot] = nullptr;
            }

            for (size_t slot = 0; slot < storage.size(); slot++) {
                EXPECT_EQ(nullptr, getDdiSlot(storage, slot))
                    << describeDdiSlot(table, slot) << " is exposed for " << apiVersionToString(version);
            }
        }
    }
}

inline void givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulatedFunction(const DdiTableExpectation *tables, size_t tablesCount) {
    // Any major version other than the supported one is unsupported, older ones included - a major
    // version bump changes the DDI table layout, so filling a table for a different major would let
    // the caller read the entries at the wrong offsets.
    constexpr uint32_t supportedMajor = ZE_MAJOR_VERSION(ZE_API_VERSION_CURRENT);
    constexpr uint32_t olderMajor = supportedMajor - 1;
    constexpr uint32_t newerMajor = supportedMajor + 1;
    const ze_api_version_t unsupportedVersions[] = {
        static_cast<ze_api_version_t>(ZE_MAKE_VERSION(olderMajor, 0)),
        static_cast<ze_api_version_t>(ZE_MAKE_VERSION(newerMajor, 0)),
        ZE_API_VERSION_FORCE_UINT32,
    };

    for (const auto version : unsupportedVersions) {
        for (size_t t = 0; t < tablesCount; t++) {
            const auto &table = tables[t];
            auto storage = createDdiTableStorage(table);

            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_VERSION, table.getProcAddrTable(version, storage.data()))
                << table.name << " called with " << apiVersionToString(version);

            for (size_t slot = 0; slot < getDdiSlotsCount(table); slot++) {
                EXPECT_EQ(nullptr, getDdiSlot(storage, slot))
                    << table.name << " slot " << slot << " is exposed for " << apiVersionToString(version);
            }
        }
    }
}

inline void givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturnedFunction(const DdiTableExpectation *tables, size_t tablesCount) {
    for (size_t t = 0; t < tablesCount; t++) {
        const auto &table = tables[t];
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, table.getProcAddrTable(ZE_API_VERSION_CURRENT, nullptr)) << table.name;
    }
}

} // namespace ult
} // namespace L0
