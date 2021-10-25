/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {
class CommandStreamReceiver;
class MockDevice;
class ExecutionEnvironment;
class MemoryManager;
struct HardwareInfo;
} // namespace NEO
namespace L0 {
namespace ult {
template <typename Type>
struct Mock;
template <typename Type>
struct WhiteBox;
} // namespace ult

struct ContextImp;
struct DriverHandleImp;
struct CommandQueue;
struct CommandList;
struct Device;

class AUBFixtureL0 {
  public:
    AUBFixtureL0();
    virtual ~AUBFixtureL0();
    void SetUp();
    void SetUp(const NEO::HardwareInfo *hardwareInfo);
    void TearDown();
    static void prepareCopyEngines(NEO::MockDevice &device, const std::string &filename);

    const uint32_t rootDeviceIndex = 0;
    NEO::ExecutionEnvironment *executionEnvironment;
    NEO::MemoryManager *memoryManager = nullptr;
    NEO::MockDevice *neoDevice = nullptr;

    std::unique_ptr<ult::Mock<DriverHandleImp>> driverHandle;
    std::unique_ptr<ult::WhiteBox<L0::CommandList>> commandList;

    Device *device = nullptr;
    ContextImp *context = nullptr;
    CommandQueue *pCmdq = nullptr;

    NEO::CommandStreamReceiver *csr = nullptr;
};

} // namespace L0