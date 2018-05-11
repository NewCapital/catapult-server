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

#pragma once
#include "MapperInclude.h"
#include <functional>

namespace catapult { namespace state { struct AccountState; } }

namespace catapult { namespace mongo { namespace mappers {

	/// Prototype for creating account states around an address and a height.
	using AccountStateFactory = std::function<state::AccountState& (const Address&, Height)>;

	/// Maps an account state (\a accountState) to the corresponding db model value.
	bsoncxx::document::value ToDbModel(const state::AccountState& accountState);

	/// Creates account state using \a accountStateFactory out of \a document.
	void ToAccountState(const bsoncxx::document::view& document, const AccountStateFactory& accountStateFactory);
}}}
