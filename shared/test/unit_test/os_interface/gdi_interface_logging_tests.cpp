/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/windows/gdi_interface_logging.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include <cstdio>

namespace NEO {
namespace GdiLogging {
extern FILE *output;
extern bool enabledLogging;
} // namespace GdiLogging
} // namespace NEO

struct GdiInterfaceLoggingBaseFixture {
    void setUp() {
        mockFopenCalledBackup = std::make_unique<VariableBackup<uint32_t>>(&NEO::IoFunctions::mockFopenCalled, 0);
        mockFcloseCalledBackup = std::make_unique<VariableBackup<uint32_t>>(&NEO::IoFunctions::mockFcloseCalled, 0);
        mockVfptrinfCalledBackup = std::make_unique<VariableBackup<uint32_t>>(&NEO::IoFunctions::mockVfptrinfCalled, 0);
    }

    void tearDown() {
    }

    std::unique_ptr<VariableBackup<uint32_t>> mockFopenCalledBackup;
    std::unique_ptr<VariableBackup<uint32_t>> mockFcloseCalledBackup;
    std::unique_ptr<VariableBackup<uint32_t>> mockVfptrinfCalledBackup;
};

struct GdiInterfaceLoggingNoInitFixture : public GdiInterfaceLoggingBaseFixture {
    void setUp() {
        GdiInterfaceLoggingBaseFixture::setUp();
        openFile = debugManager.flags.LogGdiCallsToFile.get();
        debugManager.flags.LogGdiCalls.set(true);
        mockVfptrinfUseStdioFunctionBackup = std::make_unique<VariableBackup<bool>>(&NEO::IoFunctions::mockVfptrinfUseStdioFunction, !openFile);
    }

    void tearDown() {
        GdiInterfaceLoggingBaseFixture::tearDown();
    }

    DebugManagerStateRestore dbgRestorer;
    std::unique_ptr<VariableBackup<bool>> mockVfptrinfUseStdioFunctionBackup;
    NTSTATUS status = STATUS_SUCCESS;
    std::string logEnterBegin = "GDI Call ENTER ";
    std::string logExitBegin = "GDI Call EXIT STATUS: 0x";
    bool openFile = false;
};

struct GdiInterfaceLoggingFixture : public GdiInterfaceLoggingNoInitFixture {
    void setUp() {
        GdiInterfaceLoggingNoInitFixture::setUp();
        GdiLogging::init();
        if (GdiInterfaceLoggingNoInitFixture::openFile) {
            EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
        } else {
            EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);
        }
    }

    void tearDown() {
        GdiLogging::close();
        if (GdiInterfaceLoggingNoInitFixture::openFile) {
            EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
        } else {
            EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
        }
        GdiInterfaceLoggingNoInitFixture::tearDown();
    }
};

struct GdiInterfaceLoggingToFileFixture : public GdiInterfaceLoggingFixture {
    void setUp() {
        debugManager.flags.LogGdiCallsToFile.set(true);
        GdiInterfaceLoggingFixture::setUp();
    }

    void tearDown() {
        GdiInterfaceLoggingFixture::tearDown();
    }
};

using GdiInterfaceLoggingToFileTest = Test<GdiInterfaceLoggingToFileFixture>;
using GdiInterfaceLoggingBaseTest = Test<GdiInterfaceLoggingBaseFixture>;
using GdiInterfaceLoggingTest = Test<GdiInterfaceLoggingFixture>;

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingOpenAdapterFromLuidThenExpectCorrectStrings) {
    D3DKMT_OPENADAPTERFROMLUID param = {};
    param.AdapterLuid.HighPart = 0x10;
    param.AdapterLuid.LowPart = 0x20;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_OPENADAPTERFROMLUID LUID 0x"
                   << std::hex << param.AdapterLuid.HighPart
                   << " 0x" << param.AdapterLuid.LowPart
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_OPENADAPTERFROMLUID *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_OPENADAPTERFROMLUID *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCloseAdapterThenExpectCorrectStrings) {
    D3DKMT_CLOSEADAPTER param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CLOSEADAPTER"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_CLOSEADAPTER *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_CLOSEADAPTER *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingQueryAdapterInfoThenExpectCorrectStrings) {
    D3DKMT_QUERYADAPTERINFO param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_QUERYADAPTERINFO"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_QUERYADAPTERINFO *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_QUERYADAPTERINFO *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingEscapeThenExpectCorrectStrings) {
    D3DKMT_ESCAPE param = {};
    param.hDevice = 0x40;
    param.Type = D3DKMT_ESCAPE_DEVICE;
    param.Flags.Value = 0x3;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_ESCAPE Device 0x"
                   << std::hex << param.hDevice
                   << " Type " << std::dec << param.Type
                   << " Flags 0x" << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_ESCAPE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_ESCAPE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateDeviceThenExpectCorrectStrings) {
    D3DKMT_CREATEDEVICE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATEDEVICE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATEDEVICE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATEDEVICE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingDestroyDeviceThenExpectCorrectStrings) {
    D3DKMT_DESTROYDEVICE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_DESTROYDEVICE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_DESTROYDEVICE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_DESTROYDEVICE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateContextThenExpectCorrectStrings) {
    D3DKMT_CREATECONTEXT param = {};
    param.NodeOrdinal = 1;
    param.Flags.NullRendering = 1;
    param.Flags.DisableGpuTimeout = 1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATECONTEXT NodeOrdinal "
                   << param.NodeOrdinal
                   << " Flags 0x" << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATECONTEXT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATECONTEXT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingDestroyContextThenExpectCorrectStrings) {
    D3DKMT_DESTROYCONTEXT param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_DESTROYCONTEXT"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_DESTROYCONTEXT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    status = 0xC00001;
    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_DESTROYCONTEXT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateAllocationThenExpectCorrectStrings) {
    D3DKMT_CREATEALLOCATION param = {};
    UINT flagsValue = 0xF;
    memcpy_s(&param.Flags, sizeof(param.Flags), &flagsValue, sizeof(flagsValue));

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATEALLOCATION Flags 0x"
                   << std::hex << flagsValue
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATEALLOCATION *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATEALLOCATION *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingOpenResourceThenExpectCorrectStrings) {
    D3DKMT_OPENRESOURCE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_OPENRESOURCE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_OPENRESOURCE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_OPENRESOURCE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingQueryResourceInfoThenExpectCorrectStrings) {
    D3DKMT_QUERYRESOURCEINFO param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_QUERYRESOURCEINFO"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_QUERYRESOURCEINFO *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_QUERYRESOURCEINFO *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateSynchronizationObjectThenExpectCorrectStrings) {
    D3DKMT_CREATESYNCHRONIZATIONOBJECT param = {};
    param.Info.Type = D3DDDI_MONITORED_FENCE;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATESYNCHRONIZATIONOBJECT Info Type "
                   << param.Info.Type
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATESYNCHRONIZATIONOBJECT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATESYNCHRONIZATIONOBJECT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingDestroySynchronizationObjectThenExpectCorrectStrings) {
    D3DKMT_DESTROYSYNCHRONIZATIONOBJECT param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_DESTROYSYNCHRONIZATIONOBJECT"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSignalSynchronizationObjectThenExpectCorrectStrings) {
    D3DKMT_SIGNALSYNCHRONIZATIONOBJECT param = {};
    param.Flags.Value = 0x5;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SIGNALSYNCHRONIZATIONOBJECT Flags 0x"
                   << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingWaitForSynchronizationObjectThenExpectCorrectStrings) {
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECT param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_WAITFORSYNCHRONIZATIONOBJECT"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST_FROM_WDK_10_0_18328_0 D3DKMT_WAITFORSYNCHRONIZATIONOBJECT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateSynchronizationObject2ThenExpectCorrectStrings) {
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 param = {};
    param.Info.Type = D3DDDI_MONITORED_FENCE;
    param.Info.Flags.NoSignalMaxValueOnTdr = 1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATESYNCHRONIZATIONOBJECT2 Info Type "
                   << param.Info.Type
                   << " Info Flags 0x" << std::hex << param.Info.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingGetDeviceStateThenExpectCorrectStrings) {
    D3DKMT_GETDEVICESTATE param = {};
    param.StateType = D3DKMT_DEVICESTATE_EXECUTION;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_GETDEVICESTATE StateType "
                   << param.StateType
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_GETDEVICESTATE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_GETDEVICESTATE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingMakeResidentThenExpectCorrectStrings) {
    D3DDDI_MAKERESIDENT param = {};
    param.NumAllocations = 2;
    param.Flags.CantTrimFurther = 1;
    param.PagingFenceValue = 1234;
    param.NumBytesToTrim = 0x1000;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DDDI_MAKERESIDENT NumAllocations "
                   << param.NumAllocations
                   << " Flags 0x" << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DDDI_MAKERESIDENT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " PagingFenceValue " << std::dec << param.PagingFenceValue
                   << " NumBytesToTrim 0x" << std::hex << param.NumBytesToTrim
                   << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DDDI_MAKERESIDENT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingEvictThenExpectCorrectStrings) {
    D3DKMT_EVICT param = {};
    param.NumAllocations = 2;
    param.Flags.EvictOnlyIfNecessary = 1;
    param.NumBytesToTrim = 0x1000;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_EVICT NumAllocations "
                   << param.NumAllocations
                   << " Flags 0x" << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_EVICT *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " NumBytesToTrim 0x" << std::hex << param.NumBytesToTrim
                   << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_EVICT *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingWaitForSynchronizationObjectFromCpuThenExpectCorrectStrings) {
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU param = {};
    param.Flags.WaitAny = 1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU Flags 0x"
                   << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSignalSynchronizationObjectFromCpuThenExpectCorrectStrings) {
    D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU param = {};
    param.Flags.Value = 0x5;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU Flags 0x"
                   << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingWaitForSynchronizationObjectFromGpuThenExpectCorrectStrings) {
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSignalSynchronizationObjectFromGpuThenExpectCorrectStrings) {
    D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreatePagingQueueThenExpectCorrectStrings) {
    D3DKMT_CREATEPAGINGQUEUE param = {};
    param.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_BELOW_NORMAL;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATEPAGINGQUEUE Priority "
                   << param.Priority
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATEPAGINGQUEUE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATEPAGINGQUEUE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingDestroyPagingQueueThenExpectCorrectStrings) {
    D3DDDI_DESTROYPAGINGQUEUE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DDDI_DESTROYPAGINGQUEUE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DDDI_DESTROYPAGINGQUEUE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DDDI_DESTROYPAGINGQUEUE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingLock2ThenExpectCorrectStrings) {
    D3DKMT_LOCK2 param = {};
    param.Flags.Value = 0x1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_LOCK2 Flags 0x"
                   << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_LOCK2 *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_LOCK2 *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingUnlock2ThenExpectCorrectStrings) {
    D3DKMT_UNLOCK2 param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_UNLOCK2"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_UNLOCK2 *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_UNLOCK2 *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingInvalidateCacheThenExpectCorrectStrings) {
    D3DKMT_INVALIDATECACHE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_INVALIDATECACHE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_INVALIDATECACHE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_INVALIDATECACHE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingMapGpuVirtualAddressThenExpectCorrectStrings) {
    D3DDDI_MAPGPUVIRTUALADDRESS param = {};
    param.BaseAddress = 0x100000;
    param.MaximumAddress = 0xFFFFFF;
    param.MinimumAddress = 0x200000;
    param.SizeInPages = 0x10;
    param.VirtualAddress = 0xAABB00;
    param.PagingFenceValue = 1234;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DDDI_MAPGPUVIRTUALADDRESS BaseAddress 0x"
                   << std::hex << param.BaseAddress
                   << " MinimumAddress 0x" << param.MinimumAddress
                   << " MaximumAddress 0x" << param.MaximumAddress
                   << " SizeInPages 0x" << param.SizeInPages
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DDDI_MAPGPUVIRTUALADDRESS *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " VirtualAddress 0x" << std::hex << param.VirtualAddress
                   << " PagingFenceValue " << std::dec << param.PagingFenceValue
                   << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DDDI_MAPGPUVIRTUALADDRESS *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingReserveGpuVirtualAddressThenExpectCorrectStrings) {
    D3DDDI_RESERVEGPUVIRTUALADDRESS param = {};
    param.BaseAddress = 0x100000;
    param.MaximumAddress = 0xFFFFFF;
    param.MinimumAddress = 0x200000;
    param.ReservationType = D3DDDIGPUVIRTUALADDRESS_RESERVE_NO_COMMIT;
    param.Size = 0x1000;
    param.VirtualAddress = 0xAABB00;
    param.PagingFenceValue = 1234;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DDDI_RESERVEGPUVIRTUALADDRESS BaseAddress 0x"
                   << std::hex << param.BaseAddress
                   << " MinimumAddress 0x" << param.MinimumAddress
                   << " MaximumAddress 0x" << param.MaximumAddress
                   << " Size 0x" << param.Size
                   << " ReservationType " << std::dec << param.ReservationType
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DDDI_RESERVEGPUVIRTUALADDRESS *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " VirtualAddress 0x" << std::hex << param.VirtualAddress
                   << " PagingFenceValue " << std::dec << param.PagingFenceValue
                   << std::endl;

    capture.captureStdout();
    GdiLogging::logExit<D3DDDI_RESERVEGPUVIRTUALADDRESS *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingFreeGpuVirtualAddressThenExpectCorrectStrings) {
    D3DKMT_FREEGPUVIRTUALADDRESS param = {};
    param.BaseAddress = 0x100000;
    param.Size = 0x1000;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_FREEGPUVIRTUALADDRESS BaseAddress 0x"
                   << std::hex << param.BaseAddress
                   << " Size 0x" << param.Size
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_FREEGPUVIRTUALADDRESS *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_FREEGPUVIRTUALADDRESS *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingUpdateGpuVirtualAddressThenExpectCorrectStrings) {
    D3DKMT_UPDATEGPUVIRTUALADDRESS param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_UPDATEGPUVIRTUALADDRESS"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_UPDATEGPUVIRTUALADDRESS *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateContextVirtualThenExpectCorrectStrings) {
    D3DKMT_CREATECONTEXTVIRTUAL param = {};
    param.NodeOrdinal = 1;
    param.Flags.DisableGpuTimeout = 1;
    param.ClientHint = D3DKMT_CLIENTHINT_ONEAPI_LEVEL0;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATECONTEXTVIRTUAL NodeOrdinal "
                   << param.NodeOrdinal
                   << " Flags 0x" << std::hex << param.Flags.Value
                   << " ClientHint " << std::dec << param.ClientHint
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATECONTEXTVIRTUAL *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATECONTEXTVIRTUAL *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSubmitCommandThenExpectCorrectStrings) {
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};
    cmdBufferHeader.MonitorFenceValue = 0x333;

    UINT flagsValue = 2;

    D3DKMT_SUBMITCOMMAND param = {};
    param.Commands = 0xAA00;
    param.CommandLength = 0x200;
    param.pPrivateDriverData = &cmdBufferHeader;

    memcpy_s(&param.Flags, sizeof(param.Flags), &flagsValue, sizeof(flagsValue));

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SUBMITCOMMAND Commands 0x"
                   << std::hex << param.Commands
                   << " CommandLength 0x" << param.CommandLength
                   << " Flags 0x" << flagsValue
                   << " Header MonitorFenceValue 0x" << cmdBufferHeader.MonitorFenceValue
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SUBMITCOMMAND *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SUBMITCOMMAND *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingOpenSyncObjectFromNtHandle2ThenExpectCorrectStrings) {
    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 param = {};
    param.Flags.NtSecuritySharing = 1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 Flags 0x"
                   << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingOpenSyncObjectNtHandleFromNameThenExpectCorrectStrings) {
    D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingDestroyAllocation2ThenExpectCorrectStrings) {
    D3DKMT_DESTROYALLOCATION2 param = {};
    param.Flags.SynchronousDestroy = 1;
    param.Flags.SystemUseOnly = 1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_DESTROYALLOCATION2 Flags 0x"
                   << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_DESTROYALLOCATION2 *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_DESTROYALLOCATION2 *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingRegisterTrimNotificationThenExpectCorrectStrings) {
    D3DKMT_REGISTERTRIMNOTIFICATION param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_REGISTERTRIMNOTIFICATION"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_REGISTERTRIMNOTIFICATION *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_REGISTERTRIMNOTIFICATION *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingUnregisterTrimNotificationThenExpectCorrectStrings) {
    D3DKMT_UNREGISTERTRIMNOTIFICATION param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_UNREGISTERTRIMNOTIFICATION"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_UNREGISTERTRIMNOTIFICATION *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_UNREGISTERTRIMNOTIFICATION *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingOpenResourceFromNtHandleThenExpectCorrectStrings) {
    D3DKMT_OPENRESOURCEFROMNTHANDLE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_OPENRESOURCEFROMNTHANDLE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_OPENRESOURCEFROMNTHANDLE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_OPENRESOURCEFROMNTHANDLE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingQueryResourceFromNtHandleThenExpectCorrectStrings) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingCreateHwQueueThenExpectCorrectStrings) {
    D3DKMT_CREATEHWQUEUE param = {};
    param.Flags.DisableGpuTimeout = 1;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_CREATEHWQUEUE Flags 0x"
                   << std::hex << param.Flags.Value
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<D3DKMT_CREATEHWQUEUE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<D3DKMT_CREATEHWQUEUE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingDestroyHwQueueThenExpectCorrectStrings) {
    D3DKMT_DESTROYHWQUEUE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_DESTROYHWQUEUE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_DESTROYHWQUEUE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_DESTROYHWQUEUE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSubmitCommandToHwQueueThenExpectCorrectStrings) {
    D3DKMT_SUBMITCOMMANDTOHWQUEUE param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SUBMITCOMMANDTOHWQUEUE"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSetAllocationPriorityThenExpectCorrectStrings) {
    D3DKMT_SETALLOCATIONPRIORITY param = {};

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SETALLOCATIONPRIORITY"
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SETALLOCATIONPRIORITY *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SETALLOCATIONPRIORITY *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingTest, WhenGdiLoggingIsEnabledWhenLoggingSetContextSchedulingPriorityThenExpectCorrectStrings) {
    D3DKMT_SETCONTEXTSCHEDULINGPRIORITY param = {};
    param.Priority = 2;

    std::stringstream expectedOutput;
    expectedOutput << logEnterBegin
                   << "D3DKMT_SETCONTEXTSCHEDULINGPRIORITY Priority "
                   << param.Priority
                   << std::endl;

    StreamCapture capture;
    capture.captureStdout();
    GdiLogging::logEnter<CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *>(&param);
    std::string logEnterStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logEnterStr.c_str());

    expectedOutput.str(std::string());
    expectedOutput << logExitBegin
                   << std::hex << status
                   << " " << std::endl;
    capture.captureStdout();
    GdiLogging::logExit<CONST D3DKMT_SETCONTEXTSCHEDULINGPRIORITY *>(status, &param);
    std::string logExitStr = capture.getCapturedStdout();
    EXPECT_STREQ(expectedOutput.str().c_str(), logExitStr.c_str());
}

TEST_F(GdiInterfaceLoggingToFileTest, WhenGdiLoggingIsEnabledWhenLoggingAnyGdiThenExpectLogCountCorrect) {
    D3DKMT_OPENADAPTERFROMLUID param = {};

    EXPECT_EQ(IoFunctions::mockFopenReturned, GdiLogging::output);

    GdiLogging::logEnter<CONST D3DKMT_OPENADAPTERFROMLUID *>(&param);
    EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
    GdiLogging::logExit<CONST D3DKMT_OPENADAPTERFROMLUID *>(status, &param);
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
}

TEST_F(GdiInterfaceLoggingBaseTest, WhenGdiLoggingIsDisabledWhenLoggingAnyGdiThenExpectNoLogCall) {
    D3DKMT_OPENADAPTERFROMLUID param = {};

    EXPECT_FALSE(GdiLogging::enabledLogging);
    EXPECT_EQ(nullptr, GdiLogging::output);

    GdiLogging::init();
    EXPECT_FALSE(GdiLogging::enabledLogging);
    EXPECT_EQ(nullptr, GdiLogging::output);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);

    GdiLogging::logEnter<CONST D3DKMT_OPENADAPTERFROMLUID *>(&param);
    EXPECT_EQ(0u, NEO::IoFunctions::mockVfptrinfCalled);

    NTSTATUS status = 0;
    GdiLogging::logExit<CONST D3DKMT_OPENADAPTERFROMLUID *>(status, &param);
    EXPECT_EQ(0u, NEO::IoFunctions::mockVfptrinfCalled);

    GdiLogging::close();
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
    EXPECT_FALSE(GdiLogging::enabledLogging);
}
