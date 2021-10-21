/**
*** Copyright (c) 2016-2019, Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp.
*** Copyright (c) 2020-present, Jaguar0625, gimre, BloodyRookie.
*** All rights reserved.
***
*** This file is part of Catapult.
***
*** Catapult is free software: you can redistribute it and/or modify
*** it under the terms of the GNU Lesser General Public License as published by
*** the Free Software Foundation, either version 3 of the License, or
*** (at your option) any later version.
***
*** Catapult is distributed in the hope that it will be useful,
*** but WITHOUT ANY WARRANTY; without even the implied warranty of
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*** GNU Lesser General Public License for more details.
***
*** You should have received a copy of the GNU Lesser General Public License
*** along with Catapult. If not, see <http://www.gnu.org/licenses/>.
**/

#include "src/validators/Validators.h"
#include "tests/test/plugins/ValidatorTestUtils.h"
#include "tests/TestHarness.h"

namespace catapult { namespace validators {

#define TEST_CLASS PriceMessageValidatorTests

	DEFINE_COMMON_VALIDATOR_TESTS(PriceMessage,)

	namespace {
		void AssertValidationResult(ValidationResult expectedResult, uint64_t lowPrice, uint64_t highPrice) {
			// Arrange:
			auto notification = model::PriceMessageNotification(Key(), 0u, lowPrice, highPrice);
			auto pValidator = CreatePriceMessageValidator();

			// Act:
			auto result = test::ValidateNotification(*pValidator, notification);

			// Assert:
			EXPECT_EQ(expectedResult, result);
		}
	}

	TEST(TEST_CLASS, SuccessWhenValidatingNotificationWithMessageSizeLessThanMax) {
		AssertValidationResult(ValidationResult::Success, 100u, 1234u);
	}

	TEST(TEST_CLASS, SuccessWhenValidatingNotificationWithMessageSizeEqualToMax) {
		AssertValidationResult(ValidationResult::Success, 1234u, 1234u);
	}

	TEST(TEST_CLASS, FailureWhenBothPricesAreNotSet) {
		AssertValidationResult(Failure_Price_lowPrice_and_highPrice_not_set, 0u, 0u);
	}

	TEST(TEST_CLASS, FailureWhenHighPriceIsNotSet) {
		AssertValidationResult(Failure_Price_highPrice_not_set, 1u, 0u);
	}

	TEST(TEST_CLASS, FailureWhenLowPriceIsNotSet) {
		AssertValidationResult(Failure_Price_lowPrice_not_set, 0u, 1u);
	}

	TEST(TEST_CLASS, FailureWhenLowPriceIsHigherThanHighPrice) {
		AssertValidationResult(Failure_Price_lowPrice_is_higher_than_highPrice, 2u, 1u);
	}
}}
