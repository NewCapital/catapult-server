/**
*** Copyright (c) 2016-present,
*** Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp. All rights reserved.
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

#include "Validators.h"
#include "src/cache/MosaicCache.h"
#include "catapult/cache_core/AccountStateCache.h"
#include "catapult/validators/ValidatorContext.h"

namespace catapult { namespace validators {

	using Notification = model::MosaicSupplyChangeNotification;

	DECLARE_STATEFUL_VALIDATOR(MosaicSupplyChangeAllowed, Notification)(Amount maxDivisibleUnits) {
		return MAKE_STATEFUL_VALIDATOR(MosaicSupplyChangeAllowed, [maxDivisibleUnits](
				const auto& notification,
				const ValidatorContext& context) {
			// notice that MosaicChangeAllowedValidator is required to run first, so both mosaic and owning account must exist
			const auto& cache = context.Cache.sub<cache::MosaicCache>();
			const auto& entry = cache.get(notification.MosaicId);

			const auto& accountStateCache = context.Cache.sub<cache::AccountStateCache>();
			const auto& accountState = accountStateCache.get(notification.Signer);
			auto ownerAmount = accountState.Balances.get(notification.MosaicId);

			// only allow an "immutable" supply to change if the owner owns full supply
			const auto& properties = entry.definition().properties();
			if (!properties.is(model::MosaicFlags::Supply_Mutable) && ownerAmount != entry.supply())
				return Failure_Mosaic_Supply_Immutable;

			if (model::MosaicSupplyChangeDirection::Decrease == notification.Direction)
				return ownerAmount < notification.Delta ? Failure_Mosaic_Supply_Negative : ValidationResult::Success;

			// check that new supply does not overflow and is not too large
			auto initialSupply = entry.supply();
			auto newSupply = entry.supply() + notification.Delta;
			return newSupply < initialSupply || newSupply > maxDivisibleUnits
					? Failure_Mosaic_Supply_Exceeded
					: ValidationResult::Success;
		});
	}
}}
