/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/numa_library.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include <fcntl.h>

using namespace NEO;
using namespace NEO::Linux;

struct WhiteBoxNumaLibrary : NumaLibrary {
  public:
    using NumaLibrary::numaLibNameStr;
    using NumaLibrary::procGetMemPolicyStr;
    using NumaLibrary::procNumaAvailableStr;
    using NumaLibrary::procNumaMaxNodeStr;
    using GetMemPolicyPtr = NumaLibrary::GetMemPolicyPtr;
    using NumaAvailablePtr = NumaLibrary::NumaAvailablePtr;
    using NumaMaxNodePtr = NumaLibrary::NumaMaxNodePtr;
    using NumaLibrary::getMemPolicyFunction;
    using NumaLibrary::osLibrary;
};

TEST(NumaLibraryTests, givenNumaLibraryWithValidMockOsLibraryWhenCallingGetMemPolicyThenZeroIsReturned) {
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *, unsigned long[], unsigned long, void *, unsigned long) -> long { return 0; };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return 4; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    // register proc pointers
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);

    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    EXPECT_TRUE(WhiteBoxNumaLibrary::init());
    EXPECT_TRUE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(reinterpret_cast<WhiteBoxNumaLibrary::GetMemPolicyPtr>(memPolicyHandler), WhiteBoxNumaLibrary::getMemPolicyFunction);
    std::vector<unsigned long> memPolicyNodeMask;
    int mode = -1;
    EXPECT_EQ(true, WhiteBoxNumaLibrary::getMemPolicy(&mode, memPolicyNodeMask));

    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(NumaLibraryTests, givenNumaLibraryWithInvalidMockOsLibraryWhenCallingLoadThenFalseIsReturned) {
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *, unsigned long[], unsigned long, void *, unsigned long) -> long { return 0; };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return -1; };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler2 =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return 0; };

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = nullptr;
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = nullptr;
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = nullptr;

    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, MockOsLibrary::loadLibraryNewObject);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = nullptr;
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = nullptr;
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, MockOsLibrary::loadLibraryNewObject);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = nullptr;
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, MockOsLibrary::loadLibraryNewObject);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, MockOsLibrary::loadLibraryNewObject);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler2);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, MockOsLibrary::loadLibraryNewObject);

    MockOsLibrary::loadLibraryNewObject = nullptr;
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, WhiteBoxNumaLibrary::osLibrary.get());
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(NumaLibraryTests, givenNumaLibraryWithInvalidOsLibraryWhenCallingGetMemPolicyThenErrorIsReturned) {
    MockOsLibrary::loadLibraryNewObject = new MockOsLibrary(nullptr, false);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());

    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(NumaLibraryTests, givenNumaLibraryWithInvalidGetMemPolicyWhenCallingGetMemPolicyThenErrorIsReturned) {
    MockOsLibrary::loadLibraryNewObject = new MockOsLibrary(nullptr, true);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    EXPECT_FALSE(WhiteBoxNumaLibrary::init());
    EXPECT_FALSE(WhiteBoxNumaLibrary::isLoaded());
    EXPECT_EQ(nullptr, WhiteBoxNumaLibrary::getMemPolicyFunction);
    std::vector<unsigned long> memPolicyNodeMask;
    int mode = -1;
    EXPECT_EQ(false, WhiteBoxNumaLibrary::getMemPolicy(&mode, memPolicyNodeMask));
    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}
