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

#include "Observers.h"
#include "catapult/config/CatapultDataDirectory.h"
#include "catapult/io/FileQueue.h"
#include "catapult/io/PodIoUtils.h"
#include "priceUtil.h"
#include "src/catapult/model/NetworkIdentifier.h"
#include "src/catapult/model/Address.h"

namespace catapult { namespace observers {

	namespace {
		using Notification = model::PriceMessageNotification;
	}

	DEFINE_OBSERVER(PriceMessage, Notification, [](
		const Notification& notification,
		const ObserverContext& context) {
		int identifier = plugins::networkIdentifier == "Public" ? 0x68 : 0x98;

		if (model::PublicKeyToAddressString(notification.SenderPublicKey, static_cast<model::NetworkIdentifier>(identifier)) == 
				plugins::pricePublisherAddress) {
			catapult::plugins::processPriceTransaction(notification.blockHeight, notification.lowPrice,
				notification.highPrice, context.Mode == NotifyMode::Rollback);
		}
	})
}}
