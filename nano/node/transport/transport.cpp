#include <nano/lib/logger_mt.hpp>
#include <nano/lib/rsnano.hpp>
#include <nano/node/common.hpp>
#include <nano/node/transport/inproc.hpp>
#include <nano/node/transport/transport.hpp>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <boost/format.hpp>

#include <numeric>

nano::endpoint nano::transport::map_endpoint_to_v6 (nano::endpoint const & endpoint_a)
{
	auto endpoint_l (endpoint_a);
	if (endpoint_l.address ().is_v4 ())
	{
		endpoint_l = nano::endpoint (boost::asio::ip::address_v6::v4_mapped (endpoint_l.address ().to_v4 ()), endpoint_l.port ());
	}
	return endpoint_l;
}

nano::endpoint nano::transport::map_tcp_to_endpoint (nano::tcp_endpoint const & endpoint_a)
{
	return nano::endpoint (endpoint_a.address (), endpoint_a.port ());
}

nano::tcp_endpoint nano::transport::map_endpoint_to_tcp (nano::endpoint const & endpoint_a)
{
	return nano::tcp_endpoint (endpoint_a.address (), endpoint_a.port ());
}

boost::asio::ip::address nano::transport::map_address_to_subnetwork (boost::asio::ip::address const & address_a)
{
	debug_assert (address_a.is_v6 ());
	static short const ipv6_subnet_prefix_length = 32; // Equivalent to network prefix /32.
	static short const ipv4_subnet_prefix_length = (128 - 32) + 24; // Limits for /24 IPv4 subnetwork
	return address_a.to_v6 ().is_v4_mapped () ? boost::asio::ip::make_network_v6 (address_a.to_v6 (), ipv4_subnet_prefix_length).network () : boost::asio::ip::make_network_v6 (address_a.to_v6 (), ipv6_subnet_prefix_length).network ();
}

boost::asio::ip::address nano::transport::ipv4_address_or_ipv6_subnet (boost::asio::ip::address const & address_a)
{
	debug_assert (address_a.is_v6 ());
	static short const ipv6_address_prefix_length = 48; // /48 IPv6 subnetwork
	return address_a.to_v6 ().is_v4_mapped () ? address_a : boost::asio::ip::make_network_v6 (address_a.to_v6 (), ipv6_address_prefix_length).network ();
}

nano::transport::channel::channel (rsnano::ChannelHandle * handle_a) :
	handle (handle_a)
{
}

nano::transport::channel::~channel ()
{
	rsnano::rsn_channel_destroy (handle);
}

bool nano::transport::channel::is_temporary () const
{
	return rsnano::rsn_channel_is_temporary (handle);
}

void nano::transport::channel::set_temporary (bool temporary)
{
	rsnano::rsn_channel_set_temporary (handle, temporary);
}

std::chrono::steady_clock::time_point nano::transport::channel::get_last_bootstrap_attempt () const
{
	auto value = rsnano::rsn_channel_get_last_bootstrap_attempt (handle);
	return std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (value));
}

void nano::transport::channel::set_last_bootstrap_attempt (std::chrono::steady_clock::time_point const time_a)
{
	rsnano::rsn_channel_set_last_bootstrap_attempt (handle, time_a.time_since_epoch ().count ());
}

std::chrono::steady_clock::time_point nano::transport::channel::get_last_packet_received () const
{
	auto value = rsnano::rsn_channel_get_last_packet_received (handle);
	return std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (value));
}

void nano::transport::channel::set_last_packet_sent (std::chrono::steady_clock::time_point const time_a)
{
	rsnano::rsn_channel_set_last_packet_sent (handle, time_a.time_since_epoch ().count ());
}

std::chrono::steady_clock::time_point nano::transport::channel::get_last_packet_sent () const
{
	auto value = rsnano::rsn_channel_get_last_packet_sent (handle);
	return std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (value));
}

void nano::transport::channel::set_last_packet_received (std::chrono::steady_clock::time_point const time_a)
{
	rsnano::rsn_channel_set_last_packet_received (handle, time_a.time_since_epoch ().count ());
}

boost::optional<nano::account> nano::transport::channel::get_node_id_optional () const
{
	nano::account result;
	if (rsnano::rsn_channel_get_node_id (handle, result.bytes.data ()))
	{
		return result;
	}

	return boost::none;
}

nano::account nano::transport::channel::get_node_id () const
{
	auto node_id{ get_node_id_optional () };
	nano::account result;
	if (node_id.is_initialized ())
	{
		result = node_id.get ();
	}
	else
	{
		result = 0;
	}
	return result;
}

void nano::transport::channel::set_node_id (nano::account node_id_a)
{
	rsnano::rsn_channel_set_node_id (handle, node_id_a.bytes.data ());
}

boost::asio::ip::address_v6 nano::transport::mapped_from_v4_bytes (unsigned long address_a)
{
	return boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (address_a));
}

boost::asio::ip::address_v6 nano::transport::mapped_from_v4_or_v6 (boost::asio::ip::address const & address_a)
{
	return address_a.is_v4 () ? boost::asio::ip::address_v6::v4_mapped (address_a.to_v4 ()) : address_a.to_v6 ();
}

bool nano::transport::is_ipv4_or_v4_mapped_address (boost::asio::ip::address const & address_a)
{
	return address_a.is_v4 () || address_a.to_v6 ().is_v4_mapped ();
}

bool nano::transport::reserved_address (nano::endpoint const & endpoint_a, bool allow_local_peers)
{
	debug_assert (endpoint_a.address ().is_v6 ());
	auto bytes (endpoint_a.address ().to_v6 ());
	auto result (false);
	static auto const rfc1700_min (mapped_from_v4_bytes (0x00000000ul));
	static auto const rfc1700_max (mapped_from_v4_bytes (0x00fffffful));
	static auto const rfc1918_1_min (mapped_from_v4_bytes (0x0a000000ul));
	static auto const rfc1918_1_max (mapped_from_v4_bytes (0x0afffffful));
	static auto const rfc1918_2_min (mapped_from_v4_bytes (0xac100000ul));
	static auto const rfc1918_2_max (mapped_from_v4_bytes (0xac1ffffful));
	static auto const rfc1918_3_min (mapped_from_v4_bytes (0xc0a80000ul));
	static auto const rfc1918_3_max (mapped_from_v4_bytes (0xc0a8fffful));
	static auto const rfc6598_min (mapped_from_v4_bytes (0x64400000ul));
	static auto const rfc6598_max (mapped_from_v4_bytes (0x647ffffful));
	static auto const rfc5737_1_min (mapped_from_v4_bytes (0xc0000200ul));
	static auto const rfc5737_1_max (mapped_from_v4_bytes (0xc00002fful));
	static auto const rfc5737_2_min (mapped_from_v4_bytes (0xc6336400ul));
	static auto const rfc5737_2_max (mapped_from_v4_bytes (0xc63364fful));
	static auto const rfc5737_3_min (mapped_from_v4_bytes (0xcb007100ul));
	static auto const rfc5737_3_max (mapped_from_v4_bytes (0xcb0071fful));
	static auto const ipv4_multicast_min (mapped_from_v4_bytes (0xe0000000ul));
	static auto const ipv4_multicast_max (mapped_from_v4_bytes (0xeffffffful));
	static auto const rfc6890_min (mapped_from_v4_bytes (0xf0000000ul));
	static auto const rfc6890_max (mapped_from_v4_bytes (0xfffffffful));
	static auto const rfc6666_min (boost::asio::ip::make_address_v6 ("100::"));
	static auto const rfc6666_max (boost::asio::ip::make_address_v6 ("100::ffff:ffff:ffff:ffff"));
	static auto const rfc3849_min (boost::asio::ip::make_address_v6 ("2001:db8::"));
	static auto const rfc3849_max (boost::asio::ip::make_address_v6 ("2001:db8:ffff:ffff:ffff:ffff:ffff:ffff"));
	static auto const rfc4193_min (boost::asio::ip::make_address_v6 ("fc00::"));
	static auto const rfc4193_max (boost::asio::ip::make_address_v6 ("fd00:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
	static auto const ipv6_multicast_min (boost::asio::ip::make_address_v6 ("ff00::"));
	static auto const ipv6_multicast_max (boost::asio::ip::make_address_v6 ("ff00:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
	if (endpoint_a.port () == 0)
	{
		result = true;
	}
	else if (bytes >= rfc1700_min && bytes <= rfc1700_max)
	{
		result = true;
	}
	else if (bytes >= rfc5737_1_min && bytes <= rfc5737_1_max)
	{
		result = true;
	}
	else if (bytes >= rfc5737_2_min && bytes <= rfc5737_2_max)
	{
		result = true;
	}
	else if (bytes >= rfc5737_3_min && bytes <= rfc5737_3_max)
	{
		result = true;
	}
	else if (bytes >= ipv4_multicast_min && bytes <= ipv4_multicast_max)
	{
		result = true;
	}
	else if (bytes >= rfc6890_min && bytes <= rfc6890_max)
	{
		result = true;
	}
	else if (bytes >= rfc6666_min && bytes <= rfc6666_max)
	{
		result = true;
	}
	else if (bytes >= rfc3849_min && bytes <= rfc3849_max)
	{
		result = true;
	}
	else if (bytes >= ipv6_multicast_min && bytes <= ipv6_multicast_max)
	{
		result = true;
	}
	else if (!allow_local_peers)
	{
		if (bytes >= rfc1918_1_min && bytes <= rfc1918_1_max)
		{
			result = true;
		}
		else if (bytes >= rfc1918_2_min && bytes <= rfc1918_2_max)
		{
			result = true;
		}
		else if (bytes >= rfc1918_3_min && bytes <= rfc1918_3_max)
		{
			result = true;
		}
		else if (bytes >= rfc6598_min && bytes <= rfc6598_max)
		{
			result = true;
		}
		else if (bytes >= rfc4193_min && bytes <= rfc4193_max)
		{
			result = true;
		}
	}
	return result;
}

using namespace std::chrono_literals;

nano::bandwidth_limiter::bandwidth_limiter (double const limit_burst_ratio_a, std::size_t const limit_a)
{
	handle = rsnano::rsn_bandwidth_limiter_create (limit_burst_ratio_a, limit_a);
}

nano::bandwidth_limiter::bandwidth_limiter (rsnano::BandwidthLimiterHandle * handle_a) :
	handle{ handle_a }
{
}

nano::bandwidth_limiter::bandwidth_limiter (nano::bandwidth_limiter && other_a) :
	handle{ other_a.handle }
{
	other_a.handle = nullptr;
}

nano::bandwidth_limiter::~bandwidth_limiter ()
{
	if (handle)
		rsnano::rsn_bandwidth_limiter_destroy (handle);
}

bool nano::bandwidth_limiter::should_drop (std::size_t const & message_size_a)
{
	int32_t result;
	auto should = rsnano::rsn_bandwidth_limiter_should_drop (handle, message_size_a, &result);
	if (result < 0)
	{
		throw std::runtime_error ("should_drop failed");
	}

	return should;
}

void nano::bandwidth_limiter::reset (double const limit_burst_ratio_a, std::size_t const limit_a)
{
	rsnano::rsn_bandwidth_limiter_reset (handle, limit_burst_ratio_a, limit_a);
}
