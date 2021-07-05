/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "third_party/gtest/gtest/gtest.h"

namespace L0 {

class AUBFixtureL0 {
  public:
    void SetUp(const HardwareInfo *hardwareInfo);
    void TearDown();
    static void prepareCopyEngines(NEO::MockDevice &device, const std::string &filename);

    const uint32_t rootDeviceIndex = 0;
    NEO::ExecutionEnvironment *executionEnvironment;
    NEO::MemoryManager *memoryManager = nullptr;
    NEO::MockDevice *neoDevice = nullptr;

    std::unique_ptr<ult::Mock<DriverHandleImp>> driverHandle;
    std::unique_ptr<ult::CommandList> commandList;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;

    Device *device = nullptr;
    ContextImp *context = nullptr;
    CommandQueue *pCmdq = nullptr;

    CommandStreamReceiver *csr = nullptr;
};

} // namespace L0