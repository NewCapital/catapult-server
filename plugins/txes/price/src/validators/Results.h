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

#pragma once
#ifndef CUSTOM_RESULT_DEFINITION
#include "catapult/validators/ValidationResult.h"

namespace catapult { namespace validators {

#endif
/// Defines a transfer validation result with \a DESCRIPTION and \a CODE.
#define DEFINE_PRICE_RESULT(DESCRIPTION, CODE) DEFINE_VALIDATION_RESULT(Failure, Price, DESCRIPTION, CODE, None)

	DEFINE_PRICE_RESULT(lowPrice_not_set, 1);
	DEFINE_PRICE_RESULT(highPrice_not_set, 2);
	DEFINE_PRICE_RESULT(lowPrice_and_highPrice_not_set, 3);
	DEFINE_PRICE_RESULT(lowPrice_is_higher_than_highPrice, 4);

#ifndef CUSTOM_RESULT_DEFINITION
}}
#endif
