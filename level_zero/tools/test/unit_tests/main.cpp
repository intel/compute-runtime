/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/helpers/default_hw_info.inl"
#include "shared/test/unit_test/helpers/memory_leak_listener.h"
#include "shared/test/unit_test/helpers/ult_hw_config.inl"

#include "opencl/source/utilities/logger.h"
#include "opencl/test/unit_test/custom_event_listener.h"
#include "opencl/test/unit_test/mocks/mock_gmm_client_context.h"
#include "opencl/test/unit_test/mocks/mock_sip.h"

#include "gmock/gmock.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

#ifdef WIN32
const char *fSeparator = "\\";
#else
const char *fSeparator = "/";
#endif

TEST(Should, pass) { EXPECT_TRUE(true); }

namespace L0 {

namespace ult {
::testing::Environment *environment = nullptr;
}
} // namespace L0

std::thread::id tempThreadID;

void applyWorkarounds() {
    NEO::platformsImpl.reserve(1);
    {
        std::ofstream f;
        const std::string fileName("_tmp_");
        f.open(fileName, std::ofstream::binary);
        f.close();
    }
    {
        std::mutex mtx;
        std::unique_lock<std::mutex> stateLock(mtx);
    }
    {
        std::stringstream ss("1");
        int val;
        ss >> val;
    }
    {
        class BaseClass {
          public:
            int method(int param) { return 1; }
        };
        class MockClass : public BaseClass {
          public:
            MOCK_METHOD1(method, int(int param));
        };
        ::testing::NiceMock<MockClass> mockObj;
        EXPECT_CALL(mockObj, method(::testing::_))
            .Times(1);
        mockObj.method(2);
    }

    //intialize rand
    srand(static_cast<unsigned int>(time(nullptr)));

    //Create at least on thread to prevent false memory leaks in tests using threads
    std::thread t([&]() {
    });
    tempThreadID = t.get_id();
    t.join();

    //Create FileLogger to prevent false memory leaks
    {
        NEO::FileLoggerInstance();
    }
}

int main(int argc, char **argv) {
    bool useDefaultListener = false;
    applyWorkarounds();

    testing::InitGoogleMock(&argc, argv);

    auto &listeners = ::testing::UnitTest::GetInstance()->listeners();

    if (useDefaultListener == false) {
        ::testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        ::testing::TestEventListener *defaultListener = listeners.default_result_printer();

        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(listeners.default_result_printer());
        listeners.Append(customEventListener);
    }

    listeners.Append(new NEO::MemoryLeakListener);

    if (L0::ult::environment) {
        ::testing::AddGlobalTestEnvironment(L0::ult::environment);
    }

    auto retVal = RUN_ALL_TESTS();

    return retVal;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
