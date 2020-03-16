/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/unit_test/helpers/default_hw_info.inl"
#include "shared/test/unit_test/helpers/memory_leak_listener.h"
#include "shared/test/unit_test/helpers/ult_hw_config.inl"

#include "opencl/source/program/kernel_info.h"
#include "opencl/source/utilities/logger.h"
#include "opencl/test/unit_test/custom_event_listener.h"
#include "opencl/test/unit_test/mocks/mock_gmm_client_context.h"
#include "opencl/test/unit_test/mocks/mock_sip.h"

#include "gmock/gmock.h"
#include "igfxfmid.h"

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

using namespace L0::ult;

PRODUCT_FAMILY productFamily = IGFX_SKYLAKE;
GFXCORE_FAMILY renderCoreFamily = IGFX_GEN9_CORE;

namespace NEO {
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];
namespace MockSipData {
extern std::unique_ptr<MockSipKernel> mockSipKernel;
}
} // namespace NEO

std::string getRunPath(char *argv0) {
    std::string res(argv0);

    auto pos = res.rfind(fSeparator);
    if (pos != std::string::npos)
        res = res.substr(0, pos);

    if (res == "." || pos == std::string::npos) {
#if defined(__linux__)
        res = getcwd(nullptr, 0);
#else
        res = _getcwd(nullptr, 0);
#endif
    }

    return res;
}

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
}

int main(int argc, char **argv) {
    bool useDefaultListener = false;
    applyWorkarounds();

    testing::InitGoogleMock(&argc, argv);

    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--product", argv[i])) {
            ++i;
            if (i < argc) {
                if (::isdigit(argv[i][0])) {
                    int productValue = atoi(argv[i]);
                    if (productValue > 0 && productValue < IGFX_MAX_PRODUCT &&
                        NEO::hardwarePrefix[productValue] != nullptr) {
                        ::productFamily = static_cast<PRODUCT_FAMILY>(productValue);
                    } else {
                        ::productFamily = IGFX_UNKNOWN;
                    }
                } else {
                    ::productFamily = IGFX_UNKNOWN;
                    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
                        if (NEO::hardwarePrefix[j] == nullptr)
                            continue;
                        if (strcmp(NEO::hardwarePrefix[j], argv[i]) == 0) {
                            ::productFamily = static_cast<PRODUCT_FAMILY>(j);
                            break;
                        }
                    }
                }
                if (::productFamily == IGFX_UNKNOWN) {
                    std::cout << "unknown or unsupported product family has been set: " << argv[i]
                              << std::endl;
                    return -1;
                } else {
                    std::cout << "product family: " << NEO::hardwarePrefix[::productFamily] << " ("
                              << ::productFamily << ")" << std::endl;
                }
            }
        }
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        }
    }
    auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (useDefaultListener == false) {
        auto defaultListener = listeners.default_result_printer();

        auto customEventListener = new CCustomEventListener(defaultListener, NEO::hardwarePrefix[::productFamily]);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    listeners.Append(new NEO::MemoryLeakListener);

    NEO::GmmHelper::createGmmContextWrapperFunc =
        NEO::GmmClientContextBase::create<NEO::MockGmmClientContext>;

    if (environment) {
        ::testing::AddGlobalTestEnvironment(environment);
    }

    PLATFORM platform;
    auto hardwareInfo = NEO::hardwareInfoTable[productFamily];
    if (!hardwareInfo) {
        return -1;
    }
    platform = hardwareInfo->platform;

    NEO::useKernelDescriptor = true;
    NEO::MockSipData::mockSipKernel.reset(new NEO::MockSipKernel());

    return RUN_ALL_TESTS();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
