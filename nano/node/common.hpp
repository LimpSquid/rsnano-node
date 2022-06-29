#pragma once

#include <nano/boost/asio/ip/tcp.hpp>
#include <nano/boost/asio/ip/udp.hpp>
#include <nano/crypto_lib/random_pool.hpp>
#include <nano/lib/asio.hpp>
#include <nano/lib/jsonconfig.hpp>
#include <nano/lib/memory.hpp>
#include <nano/secure/common.hpp>
#include <nano/secure/network_filter.hpp>

#include <bitset>

namespace nano
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address (std::string const &, boost::asio::ip::address &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, nano::endpoint &);
bool parse_tcp_endpoint (std::string const &, nano::tcp_endpoint &);
uint64_t ip_address_hash_raw (boost::asio::ip::address const & ip_a, uint16_t port = 0);
}

namespace
{
uint64_t endpoint_hash_raw (nano::endpoint const & endpoint_a)
{
	uint64_t result (nano::ip_address_hash_raw (endpoint_a.address (), endpoint_a.port ()));
	return result;
}
uint64_t endpoint_hash_raw (nano::tcp_endpoint const & endpoint_a)
{
	uint64_t result (nano::ip_address_hash_raw (endpoint_a.address (), endpoint_a.port ()));
	return result;
}

template <std::size_t size>
struct endpoint_hash
{
};

template <>
struct endpoint_hash<8>
{
	std::size_t operator() (nano::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
	std::size_t operator() (nano::tcp_endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};

template <>
struct endpoint_hash<4>
{
	std::size_t operator() (nano::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
	std::size_t operator() (nano::tcp_endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};

template <std::size_t size>
struct ip_address_hash
{
};

template <>
struct ip_address_hash<8>
{
	std::size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		return nano::ip_address_hash_raw (ip_address_a);
	}
};

template <>
struct ip_address_hash<4>
{
	std::size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		uint64_t big (nano::ip_address_hash_raw (ip_address_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
}

namespace std
{
template <>
struct hash<::nano::endpoint>
{
	std::size_t operator() (::nano::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (std::size_t)> ehash;
		return ehash (endpoint_a);
	}
};

template <>
struct hash<::nano::tcp_endpoint>
{
	std::size_t operator() (::nano::tcp_endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (std::size_t)> ehash;
		return ehash (endpoint_a);
	}
};

#ifndef BOOST_ASIO_HAS_STD_HASH
template <>
struct hash<boost::asio::ip::address>
{
	std::size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		ip_address_hash<sizeof (std::size_t)> ihash;
		return ihash (ip_a);
	}
};
#endif
}

namespace boost
{
template <>
struct hash<::nano::endpoint>
{
	std::size_t operator() (::nano::endpoint const & endpoint_a) const
	{
		std::hash<::nano::endpoint> hash;
		return hash (endpoint_a);
	}
};

template <>
struct hash<::nano::tcp_endpoint>
{
	std::size_t operator() (::nano::tcp_endpoint const & endpoint_a) const
	{
		std::hash<::nano::tcp_endpoint> hash;
		return hash (endpoint_a);
	}
};

template <>
struct hash<boost::asio::ip::address>
{
	std::size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		std::hash<boost::asio::ip::address> hash;
		return hash (ip_a);
	}
};
}

namespace nano
{
/**
 * Message types are serialized to the network and existing values must thus never change as
 * types are added, removed and reordered in the enum.
 */
enum class message_type : uint8_t
{
	invalid = 0x0,
	not_a_type = 0x1,
	keepalive = 0x2,
	publish = 0x3,
	confirm_req = 0x4,
	confirm_ack = 0x5,
	bulk_pull = 0x6,
	bulk_push = 0x7,
	frontier_req = 0x8,
	/* deleted 0x9 */
	node_id_handshake = 0x0a,
	bulk_pull_account = 0x0b,
	telemetry_req = 0x0c,
	telemetry_ack = 0x0d
};

std::string message_type_to_string (message_type);

enum class bulk_pull_account_flags : uint8_t
{
	pending_hash_and_amount = 0x0,
	pending_address_only = 0x1,
	pending_hash_amount_and_address = 0x2
};

class message_visitor;
class message_header final
{
public:
	message_header (nano::network_constants const &, nano::message_type);
	message_header (nano::network_constants const &, nano::message_type, uint8_t version_using_a);
	message_header (bool &, nano::stream &);
	message_header (message_header const &);
	message_header (message_header &&);
	message_header (rsnano::MessageHeaderHandle * handle_a);
	~message_header ();

	message_header & operator= (message_header && other_a);
	message_header & operator= (message_header const & other_a);
	void serialize (nano::stream &) const;
	bool deserialize (nano::stream &);
	nano::block_type block_type () const;
	void block_type_set (nano::block_type);
	uint8_t count_get () const;
	void count_set (uint8_t);
	std::string to_string ();

	void flag_set (uint8_t);
	static uint8_t constexpr bulk_pull_count_present_flag = 0;
	bool bulk_pull_is_count_present () const;
	static uint8_t constexpr frontier_req_only_confirmed = 1;
	static uint8_t constexpr node_id_handshake_query_flag = 0;
	static uint8_t constexpr node_id_handshake_response_flag = 1;
	bool node_id_handshake_is_query () const;
	bool node_id_handshake_is_response () const;

	/** Size of the payload in bytes. For some messages, the payload size is based on header flags. */
	std::size_t payload_length_bytes () const;

	nano::networks get_network () const;
	uint8_t get_version_max () const;
	uint8_t get_version_using () const;
	uint8_t get_version_min () const;
	nano::message_type get_type () const;
	std::bitset<16> get_extensions () const;
	uint16_t get_extensions_raw () const;
	void set_extensions (std::bitset<16> const & bits);
	bool test_extension (std::size_t position) const;
	void set_extension (std::size_t position, bool value);

	static std::bitset<16> constexpr telemetry_size_mask{ 0x3ff };

	rsnano::MessageHeaderHandle * handle;

	static std::size_t size ();
};

class message
{
public:
	explicit message (rsnano::MessageHandle * handle);
	message (message const &) = delete;
	message (message &&) = delete;
	virtual ~message ();
	message & operator= (message const &) = delete;
	message & operator= (message &&) = delete;
	virtual void serialize (nano::stream &) const = 0;
	virtual void visit (nano::message_visitor &) const = 0;
	std::shared_ptr<std::vector<uint8_t>> to_bytes () const;
	nano::shared_const_buffer to_shared_const_buffer () const;
	nano::message_header get_header () const;
	void set_header (nano::message_header const & header);

protected:
	rsnano::MessageHandle * handle;
};

class work_pool;
class network_constants;
class message_parser final
{
public:
	enum class parse_status
	{
		success,
		insufficient_work,
		invalid_header,
		invalid_message_type,
		invalid_keepalive_message,
		invalid_publish_message,
		invalid_confirm_req_message,
		invalid_confirm_ack_message,
		invalid_node_id_handshake_message,
		invalid_telemetry_req_message,
		invalid_telemetry_ack_message,
		outdated_version,
		duplicate_publish_message
	};
	message_parser (nano::network_filter &, nano::block_uniquer &, nano::vote_uniquer &, nano::message_visitor &, nano::work_pool &, nano::network_constants const & protocol);
	void deserialize_buffer (uint8_t const *, std::size_t);
	void deserialize_keepalive (nano::stream &, nano::message_header const &);
	void deserialize_publish (nano::stream &, nano::message_header const &, nano::uint128_t const & = 0);
	void deserialize_confirm_req (nano::stream &, nano::message_header const &);
	void deserialize_confirm_ack (nano::stream &, nano::message_header const &);
	void deserialize_node_id_handshake (nano::stream &, nano::message_header const &);
	void deserialize_telemetry_req (nano::stream &, nano::message_header const &);
	void deserialize_telemetry_ack (nano::stream &, nano::message_header const &);
	bool at_end (nano::stream &);
	nano::network_filter & publish_filter;
	nano::block_uniquer & block_uniquer;
	nano::vote_uniquer & vote_uniquer;
	nano::message_visitor & visitor;
	nano::work_pool & pool;
	parse_status status;
	nano::network_constants const & network;
	std::string status_string ();
	static std::size_t const max_safe_udp_message_size;
};

class keepalive final : public message
{
public:
	explicit keepalive (nano::network_constants const & constants);
	explicit keepalive (nano::network_constants const & constants, uint8_t version_using_a);
	keepalive (keepalive const & other_a);
	keepalive (bool &, nano::stream &, nano::message_header const &);
	void visit (nano::message_visitor &) const override;
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	bool operator== (nano::keepalive const &) const;
	std::array<nano::endpoint, 8> get_peers () const;
	void set_peers (std::array<nano::endpoint, 8> const & peers_a);
	static std::size_t size ();
};

class publish final : public message
{
public:
	publish (bool &, nano::stream &, nano::message_header const &, nano::uint128_t const & = 0, nano::block_uniquer * = nullptr);
	publish (nano::network_constants const & constants, std::shared_ptr<nano::block> const &);
	publish (nano::publish const & other_a);
	void visit (nano::message_visitor &) const override;
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &, nano::block_uniquer * = nullptr);
	bool operator== (nano::publish const &) const;
	std::shared_ptr<nano::block> get_block () const;
	nano::uint128_t get_digest () const;
	void set_digest (nano::uint128_t digest_a);
};

class confirm_req final : public message
{
public:
	confirm_req (bool &, nano::stream &, nano::message_header const &, nano::block_uniquer * = nullptr);
	confirm_req (nano::network_constants const & constants, std::shared_ptr<nano::block> const &);
	confirm_req (nano::network_constants const & constants, std::vector<std::pair<nano::block_hash, nano::root>> const &);
	confirm_req (nano::network_constants const & constants, nano::block_hash const &, nano::root const &);
	confirm_req (nano::confirm_req const & other_a);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &, nano::block_uniquer * = nullptr);
	void visit (nano::message_visitor &) const override;
	bool operator== (nano::confirm_req const &) const;
	std::string roots_string () const;
	static std::size_t size (nano::block_type, std::size_t = 0);
	std::shared_ptr<nano::block> get_block () const;
	std::vector<std::pair<nano::block_hash, nano::root>> get_roots_hashes () const;
};

class confirm_ack final : public message
{
public:
	confirm_ack (bool &, nano::stream &, nano::message_header const &, nano::vote_uniquer * = nullptr);
	confirm_ack (nano::network_constants const & constants, std::shared_ptr<nano::vote> const &);
	confirm_ack (nano::confirm_ack const & other_a);
	void serialize (nano::stream &) const override;
	void visit (nano::message_visitor &) const override;
	bool operator== (nano::confirm_ack const &) const;
	static std::size_t size (std::size_t count);
	std::shared_ptr<nano::vote> get_vote () const;
};

class frontier_req final : public message
{
public:
	explicit frontier_req (nano::network_constants const & constants);
	frontier_req (bool &, nano::stream &, nano::message_header const &);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	void visit (nano::message_visitor &) const override;
	bool operator== (nano::frontier_req const &) const;
	bool is_only_confirmed_present () const;
	static std::size_t size ();
	nano::account get_start () const;
	void set_start (nano::account const & account);
	uint32_t get_age () const;
	void set_age (uint32_t age);
	uint32_t get_count () const;
	void set_count (uint32_t count);
};

enum class telemetry_maker : uint8_t
{
	nf_node = 0,
	nf_pruned_node = 1
};

class telemetry_data
{
public:
	telemetry_data ();
	telemetry_data (nano::telemetry_data const & other_a);
	telemetry_data (nano::telemetry_data && other_a);
	~telemetry_data ();
	nano::telemetry_data & operator= (nano::telemetry_data const & other_a);

	nano::signature get_signature () const;
	void set_signature (nano::signature const & signature_a);
	nano::account get_node_id () const;
	void set_node_id (nano::account const & node_id_a);
	uint64_t get_block_count () const;
	void set_block_count (uint64_t count_a);
	uint64_t get_cemented_count () const;
	void set_cemented_count (uint64_t count_a);
	uint64_t get_unchecked_count () const;
	void set_unchecked_count (uint64_t count_a);
	uint64_t get_account_count () const;
	void set_account_count (uint64_t count_a);
	uint64_t get_bandwidth_cap () const;
	void set_bandwidth_cap (uint64_t cap_a);
	uint64_t get_uptime () const;
	void set_uptime (uint64_t uptime_a);
	uint32_t get_peer_count () const;
	void set_peer_count (uint32_t count_a);
	uint8_t get_protocol_version () const;
	void set_protocol_version (uint8_t version_a);
	nano::block_hash get_genesis_block () const;
	void set_genesis_block (nano::block_hash const & block_a);
	uint8_t get_major_version () const;
	void set_major_version (uint8_t version_a);
	uint8_t get_minor_version () const;
	void set_minor_version (uint8_t version_a);
	uint8_t get_patch_version () const;
	void set_patch_version (uint8_t version_a);
	uint8_t get_pre_release_version () const;
	void set_pre_release_version (uint8_t version_a);
	uint8_t get_maker () const;
	void set_maker (uint8_t maker_a);
	std::chrono::system_clock::time_point get_timestamp () const;
	void set_timestamp (std::chrono::system_clock::time_point timestamp_a);
	uint64_t get_active_difficulty () const;
	void set_active_difficulty (uint64_t difficulty_a);
	std::vector<uint8_t> get_unknown_data () const;
	void set_unknown_data (std::vector<uint8_t> data_a);

private:

public:
	void serialize (nano::stream &) const;
	void deserialize (nano::stream &, uint16_t);
	nano::error serialize_json (nano::jsonconfig &, bool) const;
	nano::error deserialize_json (nano::jsonconfig &, bool);
	void sign (nano::keypair const &);
	bool validate_signature () const;
	bool operator== (nano::telemetry_data const &) const;
	bool operator!= (nano::telemetry_data const &) const;
	std::string to_string () const;

	// Size does not include unknown_data
	static std::size_t size ();
	static std::size_t latest_size ()
	{
		return size ();
	}; // This needs to be updated for each new telemetry version
	rsnano::TelemetryDataHandle * handle;

private:
	void serialize_without_signature (nano::stream &) const;
};

class telemetry_req final : public message
{
public:
	explicit telemetry_req (nano::network_constants const & constants);
	explicit telemetry_req (nano::message_header const &);
	telemetry_req (nano::telemetry_req const &);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	void visit (nano::message_visitor &) const override;
};

class telemetry_ack final : public message
{
public:
	explicit telemetry_ack (nano::network_constants const & constants);
	telemetry_ack (bool &, nano::stream &, nano::message_header const &);
	telemetry_ack (nano::network_constants const & constants, telemetry_data const &);
	telemetry_ack (nano::telemetry_ack const &);
	telemetry_ack & operator= (telemetry_ack const & other_a);
	void serialize (nano::stream &) const override;
	void visit (nano::message_visitor &) const override;
	bool deserialize (nano::stream &);
	uint16_t size () const;
	bool is_empty_payload () const;
	static uint16_t size (nano::message_header const &);

	nano::telemetry_data data;
};

class bulk_pull final : public message
{
public:
	using count_t = uint32_t;
	explicit bulk_pull (nano::network_constants const & constants);
	bulk_pull (bool &, nano::stream &, nano::message_header const &);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	void visit (nano::message_visitor &) const override;
	bool is_count_present () const;
	void set_count_present (bool);
	static std::size_t constexpr count_present_flag = nano::message_header::bulk_pull_count_present_flag;
	static std::size_t constexpr extended_parameters_size = 8;
	static std::size_t size ();
	nano::hash_or_account get_start () const;
	nano::block_hash get_end () const;
	count_t get_count () const;
	void set_start (nano::hash_or_account start_a);
	void set_end (nano::block_hash end_a);
	void set_count (count_t count_a);
};

class bulk_pull_account final : public message
{
public:
	explicit bulk_pull_account (nano::network_constants const & constants);
	bulk_pull_account (bool &, nano::stream &, nano::message_header const &);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	void visit (nano::message_visitor &) const override;
	static std::size_t size ();
	nano::account get_account () const;
	nano::amount get_minimum_amount () const;
	bulk_pull_account_flags get_flags () const;
	void set_account (nano::account account_a);
	void set_minimum_amount (nano::amount amount_a);
	void set_flags (bulk_pull_account_flags flags_a);
};

class bulk_push final : public message
{
public:
	explicit bulk_push (nano::network_constants const & constants);
	explicit bulk_push (nano::message_header const &);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	void visit (nano::message_visitor &) const override;
};

class node_id_handshake final : public message
{
public:
	node_id_handshake (bool &, nano::stream &, nano::message_header const &);
	node_id_handshake (nano::network_constants const & constants, boost::optional<nano::uint256_union>, boost::optional<std::pair<nano::account, nano::signature>>);
	node_id_handshake (node_id_handshake const &);
	void serialize (nano::stream &) const override;
	bool deserialize (nano::stream &);
	void visit (nano::message_visitor &) const override;
	bool operator== (nano::node_id_handshake const &) const;
	boost::optional<nano::uint256_union> query;
	boost::optional<std::pair<nano::account, nano::signature>> response;
	std::size_t size () const;
	static std::size_t size (nano::message_header const &);
};

class message_visitor
{
public:
	virtual void keepalive (nano::keepalive const & message)
	{
		default_handler (message);
	};
	virtual void publish (nano::publish const & message)
	{
		default_handler (message);
	}
	virtual void confirm_req (nano::confirm_req const & message)
	{
		default_handler (message);
	}
	virtual void confirm_ack (nano::confirm_ack const & message)
	{
		default_handler (message);
	}
	virtual void bulk_pull (nano::bulk_pull const & message)
	{
		default_handler (message);
	}
	virtual void bulk_pull_account (nano::bulk_pull_account const & message)
	{
		default_handler (message);
	}
	virtual void bulk_push (nano::bulk_push const & message)
	{
		default_handler (message);
	}
	virtual void frontier_req (nano::frontier_req const & message)
	{
		default_handler (message);
	}
	virtual void node_id_handshake (nano::node_id_handshake const & message)
	{
		default_handler (message);
	}
	virtual void telemetry_req (nano::telemetry_req const & message)
	{
		default_handler (message);
	}
	virtual void telemetry_ack (nano::telemetry_ack const & message)
	{
		default_handler (message);
	}
	virtual void default_handler (nano::message const &){};
	virtual ~message_visitor ();
};

class telemetry_cache_cutoffs
{
public:
	static std::chrono::seconds constexpr dev{ 3 };
	static std::chrono::seconds constexpr beta{ 15 };
	static std::chrono::seconds constexpr live{ 60 };

	static std::chrono::seconds network_to_time (network_constants const & network_constants);
};

/** Helper guard which contains all the necessary purge (remove all memory even if used) functions */
class node_singleton_memory_pool_purge_guard
{
public:
	node_singleton_memory_pool_purge_guard ();

private:
	nano::cleanup_guard cleanup_guard;
};
}
