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
#include "catapult/utils/BlockSpan.h"
#include <unordered_set>

namespace catapult { namespace utils { class ConfigurationBag; } }

namespace catapult { namespace config {

	/// Price plugin configuration settings.
	struct PriceConfiguration {
	public:
		uint64_t initialSupply;

		std::string pricePublisherPublicKey;

		uint64_t feeRecalculationFrequency;

		uint64_t multiplierRecalculationFrequency;

		uint64_t pricePeriodBlocks;

		uint64_t entryLifetime;

		uint64_t generationCeiling;

	private:
		PriceConfiguration() = default;

	public:
		/// Creates an uninitialized price configuration.
		static PriceConfiguration Uninitialized();

		/// Loads a price configuration from \a bag.
		static PriceConfiguration LoadFromBag(const utils::ConfigurationBag& bag);
	};
}}