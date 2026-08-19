/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/sharings/va/leo_cl_va_api.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_device.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <va/va_backend.h>

namespace NEO {
namespace LEO {
namespace ult {

struct VaDeviceTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();

        auto &rootDeviceEnvironment = clDevice->getDevice().getRootDeviceEnvironmentRef();
        rootDeviceEnvironment.osInterface.reset(new OSInterface);
        drm = new DrmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

        vaDisplayContext.vadpy_magic = vaDisplayMagic;
        vaDisplayContext.pDriverContext = &vaDriverContext;
        vaDriverContext.drm_state = &drmState;
    }

    VADisplay getVaDisplay() {
        return reinterpret_cast<VADisplay>(&vaDisplayContext);
    }

    static constexpr int vaDisplayMagic = 0x56414430;
    static constexpr const char *matchingPciPath = "0000:00:02.0";
    static constexpr const char *otherPciPath = "0000:4b:00.0";

    ClDevice *clDevice = nullptr;
    DrmMock *drm = nullptr;
    VADisplayContext vaDisplayContext{};
    VADriverContext vaDriverContext{};
    int drmState = 1;
};

TEST_F(VaDeviceTest, givenPciPathMatchingVaDisplayWhenGettingRootDeviceFromVaDisplayThenMatchingDeviceIsReturned) {
    drm->setPciPath(matchingPciPath);

    VADevice vaDevice{};
    EXPECT_EQ(clDevice, vaDevice.getRootDeviceFromVaDisplay(platform, getVaDisplay()));
}

TEST_F(VaDeviceTest, givenPciPathNotMatchingVaDisplayWhenGettingRootDeviceFromVaDisplayThenNullptrIsReturned) {
    drm->setPciPath(otherPciPath);

    VADevice vaDevice{};
    EXPECT_EQ(nullptr, vaDevice.getRootDeviceFromVaDisplay(platform, getVaDisplay()));
}

TEST_F(VaDeviceTest, givenUnavailableDevicePathWhenGettingRootDeviceFromVaDisplayThenNullptrIsReturned) {
    drm->setPciPath(matchingPciPath);
    VariableBackup<bool> failAccessBackup(&SysCalls::failAccess, true);

    VADevice vaDevice{};
    EXPECT_EQ(nullptr, vaDevice.getRootDeviceFromVaDisplay(platform, getVaDisplay()));
}

TEST_F(VaDeviceTest, givenPciPathWithoutDrmNodeWhenGettingRootDeviceFromVaDisplayThenNullptrIsReturned) {
    drm->setPciPath(matchingPciPath);
    VariableBackup<decltype(SysCalls::sysCallsReadlink)> readlinkBackup(&SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("../../devices/pci0000:4a/0000:00:02.0");
        strcpy_s(buf, sizeofPath, "../../devices/pci0000:4a/0000:00:02.0");
        return sizeofPath;
    });

    VADevice vaDevice{};
    EXPECT_EQ(nullptr, vaDevice.getRootDeviceFromVaDisplay(platform, getVaDisplay()));
}

TEST_F(VaDeviceTest, givenVaDisplayWhenGettingDeviceFromVaThenRootDeviceLookupResultIsForwarded) {
    drm->setPciPath(matchingPciPath);

    VADevice vaDevice{};
    EXPECT_EQ(clDevice, vaDevice.getDeviceFromVA(platform, getVaDisplay()));
}

TEST_F(VaDeviceTest, givenMatchingVaDisplayWhenGettingDeviceIdsFromVaMediaAdapterThenReturnedHandleCastsBackToSameDevice) {
    drm->setPciPath(matchingPciPath);

    cl_device_id device = nullptr;
    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(static_cast<cl_platform_id>(platform), CL_VA_API_DISPLAY_INTEL,
                                                            getVaDisplay(), CL_PREFERRED_DEVICES_FOR_VA_API_INTEL,
                                                            1, &device, &numDevices);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(clDevice, castToObject<ClDevice>(device));
}

TEST_F(VaDeviceTest, givenNonMatchingVaDisplayWhenGettingDeviceIdsFromVaMediaAdapterThenDeviceNotFoundIsReturned) {
    drm->setPciPath(otherPciPath);

    cl_device_id device = reinterpret_cast<cl_device_id>(static_cast<uintptr_t>(0x1234));
    cl_uint numDevices = 1;
    auto retVal = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(static_cast<cl_platform_id>(platform), CL_VA_API_DISPLAY_INTEL,
                                                            getVaDisplay(), CL_PREFERRED_DEVICES_FOR_VA_API_INTEL,
                                                            1, &device, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(nullptr, device);
}

TEST_F(VaDeviceTest, givenNullPlatformWhenGettingDeviceIdsFromVaMediaAdapterThenInvalidPlatformIsReturned) {
    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(nullptr, CL_VA_API_DISPLAY_INTEL,
                                                            getVaDisplay(), CL_PREFERRED_DEVICES_FOR_VA_API_INTEL,
                                                            1, nullptr, &numDevices);

    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
