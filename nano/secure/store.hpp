#pragma once

#include <nano/crypto_lib/random_pool.hpp>
#include <nano/lib/diagnosticsconfig.hpp>
#include <nano/lib/lmdbconfig.hpp>
#include <nano/lib/logger_mt.hpp>
#include <nano/lib/memory.hpp>
#include <nano/secure/buffer.hpp>
#include <nano/secure/common.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_cast.hpp>

#include <stack>

namespace nano
{
// Move to versioning with a specific version if required for a future upgrade
class block_w_sideband
{
public:
	std::shared_ptr<nano::block> block;
	nano::block_sideband sideband;
};

/**
 * Encapsulates database specific container
 */
template <typename Val>
class db_val
{
public:
	db_val (Val const & value_a) :
		value (value_a)
	{
	}

	db_val () :
		db_val (0, nullptr)
	{
	}

	db_val (std::nullptr_t) :
		db_val (0, this)
	{
	}

	db_val (nano::uint128_union const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::uint128_union *> (&val_a))
	{
	}

	db_val (nano::uint256_union const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::uint256_union *> (&val_a))
	{
	}

	db_val (nano::uint512_union const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::uint512_union *> (&val_a))
	{
	}

	db_val (nano::qualified_root const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::qualified_root *> (&val_a))
	{
	}

	db_val (nano::account_info const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			nano::vectorstream stream (*buffer);
			val_a.serialize (stream);
		}
		convert_buffer_to_value ();
	}

	db_val (nano::pending_info const & val_a) :
		db_val (val_a.db_size (), const_cast<nano::pending_info *> (&val_a))
	{
		static_assert (std::is_standard_layout<nano::pending_info>::value, "Standard layout is required");
	}

	db_val (nano::pending_key const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::pending_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<nano::pending_key>::value, "Standard layout is required");
	}

	db_val (nano::unchecked_info const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			nano::vectorstream stream (*buffer);
			val_a.serialize (stream);
		}
		convert_buffer_to_value ();
	}

	db_val (nano::unchecked_key const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::unchecked_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<nano::unchecked_key>::value, "Standard layout is required");
	}

	db_val (nano::confirmation_height_info const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			nano::vectorstream stream (*buffer);
			val_a.serialize (stream);
		}
		convert_buffer_to_value ();
	}

	db_val (nano::block_info const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::block_info *> (&val_a))
	{
		static_assert (std::is_standard_layout<nano::block_info>::value, "Standard layout is required");
	}

	db_val (nano::endpoint_key const & val_a) :
		db_val (sizeof (val_a), const_cast<nano::endpoint_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<nano::endpoint_key>::value, "Standard layout is required");
	}

	db_val (std::shared_ptr<nano::block> const & val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			nano::vectorstream stream (*buffer);
			nano::serialize_block (stream, *val_a);
		}
		convert_buffer_to_value ();
	}

	db_val (uint64_t val_a) :
		buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			boost::endian::native_to_big_inplace (val_a);
			nano::vectorstream stream (*buffer);
			nano::write (stream, val_a);
		}
		convert_buffer_to_value ();
	}

	explicit operator nano::account_info () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		nano::account_info result;
		debug_assert (size () == result.db_size ());
		bool error = result.deserialize (stream);
		debug_assert (!error);
		return result;
	}

	explicit operator nano::block_info () const
	{
		nano::block_info result;
		debug_assert (size () == sizeof (result));
		static_assert (sizeof (nano::block_info::account) + sizeof (nano::block_info::balance) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator nano::pending_info () const
	{
		nano::pending_info result;
		debug_assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator nano::pending_key () const
	{
		nano::pending_key result;
		debug_assert (size () == sizeof (result));
		static_assert (sizeof (nano::pending_key::account) + sizeof (nano::pending_key::hash) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator nano::confirmation_height_info () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		nano::confirmation_height_info result;
		bool error (result.deserialize (stream));
		(void)error;
		debug_assert (!error);
		return result;
	}

	explicit operator nano::unchecked_info () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		nano::unchecked_info result;
		bool error (result.deserialize (stream));
		(void)error;
		debug_assert (!error);
		return result;
	}

	explicit operator nano::unchecked_key () const
	{
		nano::unchecked_key result;
		debug_assert (size () == sizeof (result));
		static_assert (sizeof (nano::unchecked_key::previous) + sizeof (nano::pending_key::hash) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator nano::uint128_union () const
	{
		return convert<nano::uint128_union> ();
	}

	explicit operator nano::amount () const
	{
		return convert<nano::amount> ();
	}

	explicit operator nano::block_hash () const
	{
		return convert<nano::block_hash> ();
	}

	explicit operator nano::public_key () const
	{
		return convert<nano::public_key> ();
	}

	explicit operator nano::qualified_root () const
	{
		return convert<nano::qualified_root> ();
	}

	explicit operator nano::uint256_union () const
	{
		return convert<nano::uint256_union> ();
	}

	explicit operator nano::uint512_union () const
	{
		return convert<nano::uint512_union> ();
	}

	explicit operator std::array<char, 64> () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		std::array<char, 64> result;
		auto error = nano::try_read (stream, result);
		(void)error;
		debug_assert (!error);
		return result;
	}

	explicit operator nano::endpoint_key () const
	{
		nano::endpoint_key result;
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator block_w_sideband () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		nano::block_w_sideband block_w_sideband;
		block_w_sideband.block = (nano::deserialize_block (stream));
		auto error = block_w_sideband.sideband.deserialize (stream, block_w_sideband.block->type ());
		release_assert (!error);
		block_w_sideband.block->sideband_set (block_w_sideband.sideband);
		return block_w_sideband;
	}

	explicit operator std::nullptr_t () const
	{
		return nullptr;
	}

	explicit operator nano::no_value () const
	{
		return no_value::dummy;
	}

	explicit operator std::shared_ptr<nano::block> () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		std::shared_ptr<nano::block> result (nano::deserialize_block (stream));
		return result;
	}

	template <typename Block>
	std::shared_ptr<Block> convert_to_block () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		auto result (std::make_shared<Block> (error, stream));
		debug_assert (!error);
		return result;
	}

	explicit operator std::shared_ptr<nano::send_block> () const
	{
		return convert_to_block<nano::send_block> ();
	}

	explicit operator std::shared_ptr<nano::receive_block> () const
	{
		return convert_to_block<nano::receive_block> ();
	}

	explicit operator std::shared_ptr<nano::open_block> () const
	{
		return convert_to_block<nano::open_block> ();
	}

	explicit operator std::shared_ptr<nano::change_block> () const
	{
		return convert_to_block<nano::change_block> ();
	}

	explicit operator std::shared_ptr<nano::state_block> () const
	{
		return convert_to_block<nano::state_block> ();
	}

	explicit operator std::shared_ptr<nano::vote> () const
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		auto result (nano::make_shared<nano::vote> (error, stream));
		debug_assert (!error);
		return result;
	}

	explicit operator uint64_t () const
	{
		uint64_t result;
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (nano::try_read (stream, result));
		(void)error;
		debug_assert (!error);
		boost::endian::big_to_native_inplace (result);
		return result;
	}

	operator Val * () const
	{
		// Allow passing a temporary to a non-c++ function which doesn't have constness
		return const_cast<Val *> (&value);
	}

	operator Val const & () const
	{
		return value;
	}

	// Must be specialized
	void * data () const;
	size_t size () const;
	db_val (size_t size_a, void * data_a);
	void convert_buffer_to_value ();

	Val value;
	std::shared_ptr<std::vector<uint8_t>> buffer;

private:
	template <typename T>
	T convert () const
	{
		T result;
		debug_assert (size () == sizeof (result));
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
		return result;
	}
};

class transaction;
class store;

template <typename T, typename U>
class store_iterator_impl
{
public:
	virtual ~store_iterator_impl () = default;
	virtual nano::store_iterator_impl<T, U> & operator++ () = 0;
	virtual bool operator== (nano::store_iterator_impl<T, U> const & other_a) const = 0;
	virtual bool is_end_sentinal () const = 0;
	virtual void fill (std::pair<T, U> &) const = 0;
	nano::store_iterator_impl<T, U> & operator= (nano::store_iterator_impl<T, U> const &) = delete;
	bool operator== (nano::store_iterator_impl<T, U> const * other_a) const
	{
		return (other_a != nullptr && *this == *other_a) || (other_a == nullptr && is_end_sentinal ());
	}
	bool operator!= (nano::store_iterator_impl<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}
};
/**
 * Iterates the key/value pairs of a transaction
 */
template <typename T, typename U>
class store_iterator final
{
public:
	store_iterator (std::nullptr_t)
	{
	}
	store_iterator (std::unique_ptr<nano::store_iterator_impl<T, U>> impl_a) :
		impl (std::move (impl_a))
	{
		impl->fill (current);
	}
	store_iterator (nano::store_iterator<T, U> && other_a) :
		current (std::move (other_a.current)),
		impl (std::move (other_a.impl))
	{
	}
	nano::store_iterator<T, U> & operator++ ()
	{
		++*impl;
		impl->fill (current);
		return *this;
	}
	nano::store_iterator<T, U> & operator= (nano::store_iterator<T, U> && other_a) noexcept
	{
		impl = std::move (other_a.impl);
		current = std::move (other_a.current);
		return *this;
	}
	nano::store_iterator<T, U> & operator= (nano::store_iterator<T, U> const &) = delete;
	std::pair<T, U> * operator-> ()
	{
		return &current;
	}
	bool operator== (nano::store_iterator<T, U> const & other_a) const
	{
		return (impl == nullptr && other_a.impl == nullptr) || (impl != nullptr && *impl == other_a.impl.get ()) || (other_a.impl != nullptr && *other_a.impl == impl.get ());
	}
	bool operator!= (nano::store_iterator<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}

private:
	std::pair<T, U> current;
	std::unique_ptr<nano::store_iterator_impl<T, U>> impl;
};

// Keep this in alphabetical order
enum class tables
{
	accounts,
	blocks,
	confirmation_height,
	final_votes,
	frontiers,
	meta,
	online_weight,
	peers,
	pending,
	pruned,
	unchecked,
	vote
};

class transaction
{
public:
	virtual ~transaction () = default;
	virtual rsnano::TransactionHandle * get_rust_handle () const = 0;
};

/**
 * RAII wrapper of a read MDB_txn where the constructor starts the transaction
 * and the destructor aborts it.
 */
class read_transaction : public transaction
{
public:
	virtual void reset () = 0;
	virtual void renew () = 0;
	virtual void refresh () = 0;
};

/**
 * RAII wrapper of a read-write MDB_txn where the constructor starts the transaction
 * and the destructor commits it.
 */
class write_transaction : public transaction
{
public:
	virtual void commit () = 0;
	virtual void renew () = 0;
	virtual void refresh () = 0;
	virtual bool contains (nano::tables table_a) const = 0;
};

class ledger_cache;

/**
 * Manages frontier storage and iteration
 */
class frontier_store
{
public:
	virtual void put (nano::write_transaction const &, nano::block_hash const &, nano::account const &) = 0;
	virtual nano::account get (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual void del (nano::write_transaction const &, nano::block_hash const &) = 0;
	virtual nano::store_iterator<nano::block_hash, nano::account> begin (nano::transaction const &) const = 0;
	virtual nano::store_iterator<nano::block_hash, nano::account> begin (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual nano::store_iterator<nano::block_hash, nano::account> end () const = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::block_hash, nano::account>, nano::store_iterator<nano::block_hash, nano::account>)> const & action_a) const = 0;
};

/**
 * Manages account storage and iteration
 */
class account_store
{
public:
	virtual ~account_store (){};
	virtual void put (nano::write_transaction const &, nano::account const &, nano::account_info const &) = 0;
	virtual bool get (nano::transaction const &, nano::account const &, nano::account_info &) = 0;
	virtual void del (nano::write_transaction const &, nano::account const &) = 0;
	virtual bool exists (nano::transaction const &, nano::account const &) = 0;
	virtual size_t count (nano::transaction const &) = 0;
	virtual nano::store_iterator<nano::account, nano::account_info> begin (nano::transaction const &, nano::account const &) const = 0;
	virtual nano::store_iterator<nano::account, nano::account_info> begin (nano::transaction const &) const = 0;
	virtual nano::store_iterator<nano::account, nano::account_info> end () const = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::account, nano::account_info>, nano::store_iterator<nano::account, nano::account_info>)> const &) const = 0;
};

/**
 * Manages pending storage and iteration
 */
class pending_store
{
public:
	virtual void put (nano::write_transaction const &, nano::pending_key const &, nano::pending_info const &) = 0;
	virtual void del (nano::write_transaction const &, nano::pending_key const &) = 0;
	virtual bool get (nano::transaction const &, nano::pending_key const &, nano::pending_info &) = 0;
	virtual bool exists (nano::transaction const &, nano::pending_key const &) = 0;
	virtual bool any (nano::transaction const &, nano::account const &) = 0;
	virtual nano::store_iterator<nano::pending_key, nano::pending_info> begin (nano::transaction const &, nano::pending_key const &) const = 0;
	virtual nano::store_iterator<nano::pending_key, nano::pending_info> begin (nano::transaction const &) const = 0;
	virtual nano::store_iterator<nano::pending_key, nano::pending_info> end () const = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::pending_key, nano::pending_info>, nano::store_iterator<nano::pending_key, nano::pending_info>)> const & action_a) const = 0;
};

/**
 * Manages peer storage and iteration
 */
class peer_store
{
public:
	virtual void put (nano::write_transaction const & transaction_a, nano::endpoint_key const & endpoint_a) = 0;
	virtual void del (nano::write_transaction const & transaction_a, nano::endpoint_key const & endpoint_a) = 0;
	virtual bool exists (nano::transaction const & transaction_a, nano::endpoint_key const & endpoint_a) const = 0;
	virtual size_t count (nano::transaction const & transaction_a) const = 0;
	virtual void clear (nano::write_transaction const & transaction_a) = 0;
	virtual nano::store_iterator<nano::endpoint_key, nano::no_value> begin (nano::transaction const & transaction_a) const = 0;
	virtual nano::store_iterator<nano::endpoint_key, nano::no_value> end () const = 0;
};

/**
 * Manages online weight storage and iteration
 */
class online_weight_store
{
public:
	virtual void put (nano::write_transaction const &, uint64_t, nano::amount const &) = 0;
	virtual void del (nano::write_transaction const &, uint64_t) = 0;
	virtual nano::store_iterator<uint64_t, nano::amount> begin (nano::transaction const &) const = 0;
	virtual nano::store_iterator<uint64_t, nano::amount> rbegin (nano::transaction const &) const = 0;
	virtual nano::store_iterator<uint64_t, nano::amount> end () const = 0;
	virtual size_t count (nano::transaction const &) const = 0;
	virtual void clear (nano::write_transaction const &) = 0;
};

/**
 * Manages pruned storage and iteration
 */
class pruned_store
{
public:
	virtual void put (nano::write_transaction const & transaction_a, nano::block_hash const & hash_a) = 0;
	virtual void del (nano::write_transaction const & transaction_a, nano::block_hash const & hash_a) = 0;
	virtual bool exists (nano::transaction const & transaction_a, nano::block_hash const & hash_a) const = 0;
	virtual nano::block_hash random (nano::transaction const & transaction_a) = 0;
	virtual size_t count (nano::transaction const & transaction_a) const = 0;
	virtual void clear (nano::write_transaction const &) = 0;
	virtual nano::store_iterator<nano::block_hash, std::nullptr_t> begin (nano::transaction const & transaction_a, nano::block_hash const & hash_a) const = 0;
	virtual nano::store_iterator<nano::block_hash, std::nullptr_t> begin (nano::transaction const & transaction_a) const = 0;
	virtual nano::store_iterator<nano::block_hash, std::nullptr_t> end () const = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::block_hash, std::nullptr_t>, nano::store_iterator<nano::block_hash, std::nullptr_t>)> const & action_a) const = 0;
};

/**
 * Manages confirmation height storage and iteration
 */
class confirmation_height_store
{
public:
	virtual void put (nano::write_transaction const & transaction_a, nano::account const & account_a, nano::confirmation_height_info const & confirmation_height_info_a) = 0;

	/** Retrieves confirmation height info relating to an account.
	 *  The parameter confirmation_height_info_a is always written.
	 *  On error, the confirmation height and frontier hash are set to 0.
	 *  Ruturns true on error, false on success.
	 */
	virtual bool get (nano::transaction const & transaction_a, nano::account const & account_a, nano::confirmation_height_info & confirmation_height_info_a) = 0;

	virtual bool exists (nano::transaction const & transaction_a, nano::account const & account_a) const = 0;
	virtual void del (nano::write_transaction const & transaction_a, nano::account const & account_a) = 0;
	virtual uint64_t count (nano::transaction const & transaction_a) = 0;
	virtual void clear (nano::write_transaction const &, nano::account const &) = 0;
	virtual void clear (nano::write_transaction const &) = 0;
	virtual nano::store_iterator<nano::account, nano::confirmation_height_info> begin (nano::transaction const & transaction_a, nano::account const & account_a) const = 0;
	virtual nano::store_iterator<nano::account, nano::confirmation_height_info> begin (nano::transaction const & transaction_a) const = 0;
	virtual nano::store_iterator<nano::account, nano::confirmation_height_info> end () const = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::account, nano::confirmation_height_info>, nano::store_iterator<nano::account, nano::confirmation_height_info>)> const &) const = 0;
};

/**
 * Manages unchecked storage and iteration
 */
class unchecked_store
{
public:
	using iterator = nano::store_iterator<nano::unchecked_key, nano::unchecked_info>;

	virtual void clear (nano::write_transaction const &) = 0;
	virtual void put (nano::write_transaction const &, nano::hash_or_account const & dependency, nano::unchecked_info const &) = 0;
	std::pair<iterator, iterator> equal_range (nano::transaction const & transaction, nano::block_hash const & dependency);
	std::pair<iterator, iterator> full_range (nano::transaction const & transaction);
	virtual bool exists (nano::transaction const & transaction_a, nano::unchecked_key const & unchecked_key_a) = 0;
	virtual void del (nano::write_transaction const &, nano::unchecked_key const &) = 0;
	virtual iterator begin (nano::transaction const &) const = 0;
	virtual iterator lower_bound (nano::transaction const &, nano::unchecked_key const &) const = 0;
	virtual iterator end () const = 0;
	virtual size_t count (nano::transaction const &) = 0;
};

/**
 * Manages final vote storage and iteration
 */
class final_vote_store
{
public:
	virtual bool put (nano::write_transaction const & transaction_a, nano::qualified_root const & root_a, nano::block_hash const & hash_a) = 0;
	virtual std::vector<nano::block_hash> get (nano::transaction const & transaction_a, nano::root const & root_a) = 0;
	virtual void del (nano::write_transaction const & transaction_a, nano::root const & root_a) = 0;
	virtual size_t count (nano::transaction const & transaction_a) const = 0;
	virtual void clear (nano::write_transaction const &, nano::root const &) = 0;
	virtual void clear (nano::write_transaction const &) = 0;
	virtual nano::store_iterator<nano::qualified_root, nano::block_hash> begin (nano::transaction const & transaction_a, nano::qualified_root const & root_a) const = 0;
	virtual nano::store_iterator<nano::qualified_root, nano::block_hash> begin (nano::transaction const & transaction_a) const = 0;
	virtual nano::store_iterator<nano::qualified_root, nano::block_hash> end () const = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::qualified_root, nano::block_hash>, nano::store_iterator<nano::qualified_root, nano::block_hash>)> const & action_a) const = 0;
};

/**
 * Manages version storage
 */
class version_store
{
public:
	virtual void put (nano::write_transaction const &, int) = 0;
	virtual int get (nano::transaction const &) const = 0;
};

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
	virtual ~block_store ()
	{
	}
	virtual void put (nano::write_transaction const &, nano::block_hash const &, nano::block const &) = 0;
	virtual void raw_put (nano::write_transaction const &, std::vector<uint8_t> const &, nano::block_hash const &) = 0;
	virtual nano::block_hash successor (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual void successor_clear (nano::write_transaction const &, nano::block_hash const &) = 0;
	virtual std::shared_ptr<nano::block> get (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual std::shared_ptr<nano::block> get_no_sideband (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual std::shared_ptr<nano::block> random (nano::transaction const &) = 0;
	virtual void del (nano::write_transaction const &, nano::block_hash const &) = 0;
	virtual bool exists (nano::transaction const &, nano::block_hash const &) = 0;
	virtual uint64_t count (nano::transaction const &) = 0;
	virtual nano::account account (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual nano::account account_calculated (nano::block const &) const = 0;
	virtual nano::store_iterator<nano::block_hash, block_w_sideband> begin (nano::transaction const &, nano::block_hash const &) const = 0;
	virtual nano::store_iterator<nano::block_hash, block_w_sideband> begin (nano::transaction const &) const = 0;
	virtual nano::store_iterator<nano::block_hash, block_w_sideband> end () const = 0;
	virtual nano::uint128_t balance (nano::transaction const &, nano::block_hash const &) = 0;
	virtual nano::uint128_t balance_calculated (std::shared_ptr<nano::block> const &) const = 0;
	virtual nano::epoch version (nano::transaction const &, nano::block_hash const &) = 0;
	virtual void for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::block_hash, block_w_sideband>, nano::store_iterator<nano::block_hash, block_w_sideband>)> const & action_a) const = 0;
	virtual uint64_t account_height (nano::transaction const & transaction_a, nano::block_hash const & hash_a) const = 0;
};

class unchecked_map;
/**
 * Store manager
 */
class store
{
public:
	virtual ~store () = default;
	virtual void initialize (nano::write_transaction const & transaction_a, nano::ledger_cache & ledger_cache_a, nano::ledger_constants & constants) = 0;
	virtual block_store & block () = 0;
	virtual frontier_store & frontier () = 0;
	virtual account_store & account () = 0;
	virtual pending_store & pending () = 0;
	virtual unchecked_store & unchecked () = 0;
	virtual online_weight_store & online_weight () = 0;
	virtual pruned_store & pruned () = 0;
	virtual peer_store & peer () = 0;
	virtual confirmation_height_store & confirmation_height () = 0;
	virtual final_vote_store & final_vote () = 0;
	virtual version_store & version () = 0;
	static int constexpr version_minimum{ 21 };
	static int constexpr version_current{ 21 };

	virtual unsigned max_block_write_batch_num () const = 0;

	virtual bool copy_db (boost::filesystem::path const & destination) = 0;
	virtual void rebuild_db (nano::write_transaction const & transaction_a) = 0;

	/** Not applicable to all sub-classes */
	virtual void serialize_mdb_tracker (boost::property_tree::ptree &, std::chrono::milliseconds, std::chrono::milliseconds){};
	virtual void serialize_memory_stats (boost::property_tree::ptree &) = 0;

	virtual bool init_error () const = 0;

	/** Start read-write transaction */
	virtual std::unique_ptr<nano::write_transaction> tx_begin_write (std::vector<nano::tables> const & tables_to_lock = {}, std::vector<nano::tables> const & tables_no_lock = {}) = 0;

	/** Start read-only transaction */
	virtual std::unique_ptr<nano::read_transaction> tx_begin_read () const = 0;

	virtual std::string vendor_get () const = 0;
	virtual rsnano::LmdbStoreHandle * get_handle () const = 0;

	friend class unchecked_map;
};

std::unique_ptr<nano::store> make_store (std::shared_ptr<nano::logger_mt> logger, boost::filesystem::path const & path, nano::ledger_constants & constants, bool open_read_only = false, bool add_db_postfix = false, nano::txn_tracking_config const & txn_tracking_config_a = nano::txn_tracking_config{}, std::chrono::milliseconds block_processor_batch_max_time_a = std::chrono::milliseconds (5000), nano::lmdb_config const & lmdb_config_a = nano::lmdb_config{}, bool backup_before_upgrade = false);
}

namespace std
{
template <>
struct hash<::nano::tables>
{
	size_t operator() (::nano::tables const & table_a) const
	{
		return static_cast<size_t> (table_a);
	}
};
}
