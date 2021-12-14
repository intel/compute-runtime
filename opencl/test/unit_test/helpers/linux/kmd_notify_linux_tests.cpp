/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

class MockKmdNotifyHelper : public KmdNotifyHelper {
  public:
    using KmdNotifyHelper::getBaseTimeout;

    MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};
};

TEST(KmdNotifyLinuxTests, givenTaskCountDiffGreaterThanOneWhenBaseTimeoutRequestedThenMultiply) {
    auto localProperties = (defaultHwInfo->capabilityTable.kmdNotifyProperties);
    localProperties.delayKmdNotifyMicroseconds = 10;
    const int64_t multiplier = 10;

    MockKmdNotifyHelper helper(&localProperties);
    EXPECT_EQ(localProperties.delayKmdNotifyMicroseconds * multiplier, helper.getBaseTimeout(multiplier));
}
} // namespace NEO
