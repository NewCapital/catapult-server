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
#include "IdentifierGroupCacheUtils.h"
#include "catapult/deltaset/BaseSetDelta.h"
#include "catapult/deltaset/BaseSetIterationView.h"
#include "catapult/utils/Casting.h"
#include "catapult/utils/HexFormatter.h"
#include "catapult/utils/IdentifierGroup.h"
#include "catapult/functions.h"
#include "catapult/types.h"

namespace catapult { namespace cache {

	/// A mixin for adding size support to a cache.
	template<typename TSet>
	class SizeMixin {
	public:
		/// Creates a mixin around \a set.
		explicit SizeMixin(const TSet& set) : m_set(set)
		{}

	public:
		/// Gets the number of elements in the cache.
		size_t size() const {
			return m_set.size();
		}

	private:
		const TSet& m_set;
	};

	/// A mixin for adding contains support to a cache.
	template<typename TSet, typename TCacheDescriptor>
	class ContainsMixin {
	private:
		using KeyType = typename TCacheDescriptor::KeyType;

	public:
		/// Creates a mixin around \a set.
		explicit ContainsMixin(const TSet& set) : m_set(set)
		{}

	public:
		/// Gets a value indicating whether or not the cache contains an element with \a key.
		bool contains(const KeyType& key) const {
			return m_set.contains(key);
		}

	private:
		const TSet& m_set;
	};

	/// A mixin for adding iteration support to a cache.
	template<typename TSet>
	class IterationMixin {
	public:
		/// An iterable view of the cache.
		struct IterableView {
		public:
			/// Creates a view around \a set.
			explicit IterableView(const TSet& set) : m_set(set)
			{}

		public:
			/// Returns a const iterator to the first element of the underlying set.
			auto begin() const {
				return MakeIterableView(m_set).begin();
			}

			/// Returns a const iterator to the element following the last element of the underlying set.
			auto end() const {
				return MakeIterableView(m_set).end();
			}

		private:
			const TSet& m_set;
		};

	public:
		/// Creates a mixin around \a set.
		explicit IterationMixin(const TSet& set) : m_set(set)
		{}

	public:
		/// Creates an iterable view of the cache.
		/// \note \c nullptr will be returned if the cache does not support iteration.
		auto tryMakeIterableView() const {
			// use argument dependent lookup to resolve IsBaseSetIterable
			return IsBaseSetIterable(m_set) ? std::make_unique<IterableView>(m_set) : nullptr;
		}

	private:
		const TSet& m_set;
	};

	namespace detail {
		template<typename TCacheDescriptor, typename T>
		[[noreturn]]
		void ThrowInvalidKeyError(const char* keyState, const T& key) {
			std::ostringstream out;
			out << "value with key '" << key << "' is " << keyState << " in cache (" << TCacheDescriptor::Name << ")";
			CATAPULT_THROW_INVALID_ARGUMENT(out.str().c_str());
		}

		template<typename TCacheDescriptor, size_t N>
		[[noreturn]]
		void ThrowInvalidKeyError(const char* keyState, const std::array<uint8_t, N>& key) {
			ThrowInvalidKeyError<TCacheDescriptor>(keyState, utils::HexFormat(key));
		}

		template<typename TValue>
		struct NoOpAdapter {
			using AdaptedValueType = TValue;

			static TValue& Adapt(TValue& value) {
				return value;
			}
		};
	}

	/// A mixin for adding const access support to a cache.
	template<
		typename TSet,
		typename TCacheDescriptor,
		typename TValueAdapter = detail::NoOpAdapter<const typename TCacheDescriptor::ValueType>>
	class ConstAccessorMixin {
	private:
		using KeyType = typename TCacheDescriptor::KeyType;
		using ValueType = typename TValueAdapter::AdaptedValueType;

	public:
		/// Creates a mixin around \a set.
		explicit ConstAccessorMixin(const TSet& set) : m_set(set)
		{}

	public:
		/// Gets a const value specified by identifier \a key.
		/// \throws catapult_invalid_argument if the requested value is not found.
		const ValueType& get(const KeyType& key) const {
			const auto* pValue = tryGet(key);
			if (!pValue)
				detail::ThrowInvalidKeyError<TCacheDescriptor>("not", key);

			return *pValue;
		}

		/// Tries to get a const value specified by identifier \a key.
		const ValueType* tryGet(const KeyType& key) const {
			auto pValue = m_set.find(key); // can be raw or shared_ptr
			return pValue ? &TValueAdapter::Adapt(*pValue) : nullptr;
		}

	private:
		const TSet& m_set;
	};

	/// A mixin for adding non-const access support to a cache.
	/// \note This is not simply a specialization of ConstAccessorMixin due to differences in function constness.
	template<
		typename TSet,
		typename TCacheDescriptor,
		typename TValueAdapter = detail::NoOpAdapter<typename TCacheDescriptor::ValueType>>
	class MutableAccessorMixin {
	private:
		using KeyType = typename TCacheDescriptor::KeyType;
		using ValueType = typename TValueAdapter::AdaptedValueType;

	public:
		/// Creates a mixin around \a set.
		explicit MutableAccessorMixin(TSet& set) : m_set(set)
		{}

	public:
		/// Gets a value specified by identifier \a key.
		/// \throws catapult_invalid_argument if the requested value is not found.
		ValueType& get(const KeyType& key) {
			auto* pValue = tryGet(key);
			if (!pValue)
				detail::ThrowInvalidKeyError<TCacheDescriptor>("not", key);

			return *pValue;
		}

		/// Tries to get a value specified by identifier \a key.
		ValueType* tryGet(const KeyType& key) {
			auto pValue = m_set.find(key); // can be raw or shared_ptr
			return pValue ? &TValueAdapter::Adapt(*pValue) : nullptr;
		}

	private:
		TSet& m_set;
	};

	/// A mixin for adding active querying support to a cache.
	template<typename TSet, typename TCacheDescriptor>
	class ActivePredicateMixin {
	private:
		using KeyType = typename TCacheDescriptor::KeyType;

	public:
		/// Creates a mixin around \a set.
		explicit ActivePredicateMixin(const TSet& set) : m_set(set)
		{}

	public:
		/// Returns \c true if the value specified by identifier \a key is active at \a height.
		bool isActive(const KeyType& key, Height height) const {
			const auto* pValue = m_set.find(key);
			return pValue && pValue->isActive(height);
		}

	private:
		const TSet& m_set;
	};

	/// A mixin for adding basic insert and remove support to a cache.
	template<typename TSet, typename TCacheDescriptor>
	class BasicInsertRemoveMixin {
	private:
		using KeyType = typename TCacheDescriptor::KeyType;
		using ValueType = typename TCacheDescriptor::ValueType;

	public:
		/// Creates a mixin around \a set.
		explicit BasicInsertRemoveMixin(TSet& set) : m_set(set)
		{}

	public:
		/// Inserts \a value into the cache.
		void insert(const ValueType& value) {
			auto result = m_set.insert(value);
			if (deltaset::InsertResult::Inserted == result || deltaset::InsertResult::Unremoved == result)
				return;

			CATAPULT_LOG(error) << "insert failed with " << utils::to_underlying_type(result);
			detail::ThrowInvalidKeyError<TCacheDescriptor>("already", TCacheDescriptor::GetKeyFromValue(value));
		}

		/// Removes the value identified by \a key from the cache.
		void remove(const KeyType& key) {
			auto result = m_set.remove(key);
			if (deltaset::RemoveResult::None != result && deltaset::RemoveResult::Redundant != result)
				return;

			CATAPULT_LOG(error) << "remove failed with " << utils::to_underlying_type(result);
			detail::ThrowInvalidKeyError<TCacheDescriptor>("not", key);
		}

	private:
		TSet& m_set;
	};

	/// A mixin for height-based pruning.
	template<typename TSet, typename THeightGroupedSet>
	class HeightBasedPruningMixin {
	public:
		/// Creates a mixin around \a set and \a heightGroupedSet.
		HeightBasedPruningMixin(TSet& set, THeightGroupedSet& heightGroupedSet)
				: m_set(set)
				, m_heightGroupedSet(heightGroupedSet)
		{}

	public:
		/// Prunes the cache at \a height.
		void prune(Height height) {
			RemoveAllIdentifiersWithGroup(m_set, m_heightGroupedSet, height);
		}

	private:
		TSet& m_set;
		THeightGroupedSet& m_heightGroupedSet;
	};
}}
