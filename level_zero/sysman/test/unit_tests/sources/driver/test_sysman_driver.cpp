/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

#if defined(_WIN32)
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"
#else
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#endif

#include "gtest/gtest.h"

#include <cstring>

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanDriverHandleOsAgnosticTest = SysmanDeviceFixture;

TEST_F(SysmanDriverHandleOsAgnosticTest,
       GivenExtensionNameLongerOrEqualToMaximumCharsWhenCallingGetExtensionPropertiesThenNameIsTruncatedToMaximumCharsWithNullTermination) {
    std::string extensionNameLonger(ZES_MAX_EXTENSION_NAME, 'X');
    std::vector<std::pair<std::string, uint32_t>> extensionsSupported = {
        {extensionNameLonger, 100}};

    auto sysmanDriverHandle = static_cast<SysmanDriverHandleImp *>(driverHandle.get());

    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, sysmanDriverHandle->getExtensionProperties(&pCount, nullptr, extensionsSupported));
    EXPECT_EQ(1u, pCount);

    std::vector<zes_driver_extension_properties_t> extensionsReturned(pCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, sysmanDriverHandle->getExtensionProperties(&pCount, extensionsReturned.data(), extensionsSupported));

    // Verify that the extension name is truncated properly
    EXPECT_EQ(static_cast<size_t>(ZES_MAX_EXTENSION_NAME - 1), strlen(extensionsReturned[0].name));
    EXPECT_EQ('\0', extensionsReturned[0].name[ZES_MAX_EXTENSION_NAME - 1]);
    EXPECT_EQ(100u, extensionsReturned[0].version);
    std::string expectedName(ZES_MAX_EXTENSION_NAME - 1, 'X');
    EXPECT_EQ(expectedName, std::string(extensionsReturned[0].name));
}

} // namespace ult
} // namespace Sysman
} // namespace L0