#include <nano/node/telemetry.hpp>
#include <nano/test_common/network.hpp>
#include <nano/test_common/system.hpp>
#include <nano/test_common/telemetry.hpp>
#include <nano/test_common/testutil.hpp>

#include <gtest/gtest.h>

#include <numeric>

using namespace std::chrono_literals;

TEST (telemetry, consolidate_data)
{
	auto time = 1582117035109;

	// Pick specific values so that we can check both mode and average are working correctly
	nano::telemetry_data data;
	data.set_account_count (2);
	data.set_block_count (1);
	data.set_cemented_count (1);
	data.set_protocol_version (12);
	data.set_peer_count (2);
	data.set_bandwidth_cap (100);
	data.set_unchecked_count (3);
	data.set_uptime (6);
	data.set_genesis_block (nano::block_hash (3));
	data.set_major_version (20);
	data.set_minor_version (1);
	data.set_patch_version (4);
	data.set_pre_release_version (6);
	data.set_maker (2);
	data.set_timestamp (std::chrono::system_clock::time_point (std::chrono::milliseconds (time)));
	data.set_active_difficulty (2);

	nano::telemetry_data data1;
	data1.set_account_count (5);
	data1.set_block_count (7);
	data1.set_cemented_count (4);
	data1.set_protocol_version (11);
	data1.set_peer_count (5);
	data1.set_bandwidth_cap (0);
	data1.set_unchecked_count (1);
	data1.set_uptime (10);
	data1.set_genesis_block (nano::block_hash (4));
	data1.set_major_version (10);
	data1.set_minor_version (2);
	data1.set_patch_version (3);
	data1.set_pre_release_version (6);
	data1.set_maker (2);
	data1.set_timestamp (std::chrono::system_clock::time_point (std::chrono::milliseconds (time + 1)));
	data1.set_active_difficulty (3);

	nano::telemetry_data data2;
	data2.set_account_count (3);
	data2.set_block_count (3);
	data2.set_cemented_count (2);
	data2.set_protocol_version (11);
	data2.set_peer_count (4);
	data2.set_bandwidth_cap (0);
	data2.set_unchecked_count (2);
	data2.set_uptime (3);
	data2.set_genesis_block (nano::block_hash (4));
	data2.set_major_version (20);
	data2.set_minor_version (1);
	data2.set_patch_version (4);
	data2.set_pre_release_version (6);
	data2.set_maker (2);
	data2.set_timestamp (std::chrono::system_clock::time_point (std::chrono::milliseconds (time)));
	data2.set_active_difficulty (2);

	std::vector<nano::telemetry_data> all_data{ data, data1, data2 };

	auto consolidated_telemetry_data = nano::consolidate_telemetry_data (all_data);
	ASSERT_EQ (consolidated_telemetry_data.get_account_count (), 3);
	ASSERT_EQ (consolidated_telemetry_data.get_block_count (), 3);
	ASSERT_EQ (consolidated_telemetry_data.get_cemented_count (), 2);
	ASSERT_EQ (consolidated_telemetry_data.get_protocol_version (), 11);
	ASSERT_EQ (consolidated_telemetry_data.get_peer_count (), 3);
	ASSERT_EQ (consolidated_telemetry_data.get_bandwidth_cap (), 0);
	ASSERT_EQ (consolidated_telemetry_data.get_unchecked_count (), 2);
	ASSERT_EQ (consolidated_telemetry_data.get_uptime (), 6);
	ASSERT_EQ (consolidated_telemetry_data.get_genesis_block (), nano::block_hash (4));
	ASSERT_EQ (consolidated_telemetry_data.get_major_version (), 20);
	ASSERT_EQ (consolidated_telemetry_data.get_minor_version (), 1);
	ASSERT_EQ (consolidated_telemetry_data.get_patch_version (), 4);
	ASSERT_EQ (consolidated_telemetry_data.get_pre_release_version (), 6);
	ASSERT_EQ (consolidated_telemetry_data.get_maker (), 2);
	ASSERT_EQ (consolidated_telemetry_data.get_timestamp (), std::chrono::system_clock::time_point (std::chrono::milliseconds (time)));
	ASSERT_EQ (consolidated_telemetry_data.get_active_difficulty (), 2);

	// Modify the metrics which may be either the mode or averages to ensure all are tested.
	all_data[2].set_bandwidth_cap (53);
	all_data[2].set_protocol_version (13);
	all_data[2].set_genesis_block (nano::block_hash (3));
	all_data[2].set_major_version (10);
	all_data[2].set_minor_version (2);
	all_data[2].set_patch_version (3);
	all_data[2].set_pre_release_version (6);
	all_data[2].set_maker (2);

	auto consolidated_telemetry_data1 = nano::consolidate_telemetry_data (all_data);
	ASSERT_EQ (consolidated_telemetry_data1.get_major_version (), 10);
	ASSERT_EQ (consolidated_telemetry_data1.get_minor_version (), 2);
	ASSERT_EQ (consolidated_telemetry_data1.get_patch_version (), 3);
	ASSERT_EQ (consolidated_telemetry_data1.get_pre_release_version (), 6);
	ASSERT_EQ (consolidated_telemetry_data1.get_maker (), 2);
	ASSERT_TRUE (consolidated_telemetry_data1.get_protocol_version () == 11 || consolidated_telemetry_data1.get_protocol_version () == 12 || consolidated_telemetry_data1.get_protocol_version () == 13);
	ASSERT_EQ (consolidated_telemetry_data1.get_bandwidth_cap (), 51);
	ASSERT_EQ (consolidated_telemetry_data1.get_genesis_block (), nano::block_hash (3));

	// Test equality operator
	ASSERT_FALSE (consolidated_telemetry_data == consolidated_telemetry_data1);
	ASSERT_EQ (consolidated_telemetry_data, consolidated_telemetry_data);
}

TEST (telemetry, consolidate_data_remove_outliers)
{
	nano::telemetry_data data;
	data.set_account_count (2);
	data.set_block_count (1);
	data.set_cemented_count (1);
	data.set_protocol_version (12);
	data.set_peer_count (2);
	data.set_bandwidth_cap (100);
	data.set_unchecked_count (3);
	data.set_uptime (6);
	data.set_genesis_block (nano::block_hash (3));
	data.set_major_version (20);
	data.set_minor_version (1);
	data.set_patch_version (5);
	data.set_pre_release_version (2);
	data.set_maker (1);
	data.set_timestamp (std::chrono::system_clock::time_point (100ms));
	data.set_active_difficulty (10);

	// Insert 20 of these, and 2 outliers at the lower and upper bounds which should get removed
	std::vector<nano::telemetry_data> all_data (20, data);

	// Insert some outliers
	nano::telemetry_data lower_bound_outlier_data;
	lower_bound_outlier_data.set_account_count (1);
	lower_bound_outlier_data.set_block_count (0);
	lower_bound_outlier_data.set_cemented_count (0);
	lower_bound_outlier_data.set_protocol_version (11);
	lower_bound_outlier_data.set_peer_count (0);
	lower_bound_outlier_data.set_bandwidth_cap (8);
	lower_bound_outlier_data.set_unchecked_count (1);
	lower_bound_outlier_data.set_uptime (2);
	lower_bound_outlier_data.set_genesis_block (nano::block_hash (2));
	lower_bound_outlier_data.set_major_version (11);
	lower_bound_outlier_data.set_minor_version (1);
	lower_bound_outlier_data.set_patch_version (1);
	lower_bound_outlier_data.set_pre_release_version (1);
	lower_bound_outlier_data.set_maker (1);
	lower_bound_outlier_data.set_timestamp (std::chrono::system_clock::time_point (1ms));
	lower_bound_outlier_data.set_active_difficulty (1);
	all_data.push_back (lower_bound_outlier_data);
	all_data.push_back (lower_bound_outlier_data);

	nano::telemetry_data upper_bound_outlier_data;
	upper_bound_outlier_data.set_account_count (99);
	upper_bound_outlier_data.set_block_count (99);
	upper_bound_outlier_data.set_cemented_count (99);
	upper_bound_outlier_data.set_protocol_version (99);
	upper_bound_outlier_data.set_peer_count (99);
	upper_bound_outlier_data.set_bandwidth_cap (999);
	upper_bound_outlier_data.set_unchecked_count (99);
	upper_bound_outlier_data.set_uptime (999);
	upper_bound_outlier_data.set_genesis_block (nano::block_hash (99));
	upper_bound_outlier_data.set_major_version (99);
	upper_bound_outlier_data.set_minor_version (9);
	upper_bound_outlier_data.set_patch_version (9);
	upper_bound_outlier_data.set_pre_release_version (9);
	upper_bound_outlier_data.set_maker (9);
	upper_bound_outlier_data.set_timestamp (std::chrono::system_clock::time_point (999ms));
	upper_bound_outlier_data.set_active_difficulty (99);
	all_data.push_back (upper_bound_outlier_data);
	all_data.push_back (upper_bound_outlier_data);

	auto consolidated_telemetry_data = nano::consolidate_telemetry_data (all_data);
	ASSERT_EQ (data, consolidated_telemetry_data);
}

TEST (telemetry, consolidate_data_remove_outliers_with_zero_bandwidth)
{
	nano::telemetry_data data1;
	data1.set_account_count (2);
	data1.set_block_count (1);
	data1.set_cemented_count (1);
	data1.set_protocol_version (12);
	data1.set_peer_count (2);
	data1.set_bandwidth_cap (0);
	data1.set_unchecked_count (3);
	data1.set_uptime (6);
	data1.set_genesis_block (nano::block_hash (3));
	data1.set_major_version (20);
	data1.set_minor_version (1);
	data1.set_patch_version (5);
	data1.set_pre_release_version (2);
	data1.set_maker (1);
	data1.set_timestamp (std::chrono::system_clock::time_point (100ms));
	data1.set_active_difficulty (10);

	// Add a majority of nodes with bandwidth set to 0
	std::vector<nano::telemetry_data> all_data (100, data1);

	nano::telemetry_data data2;
	data2.set_account_count (2);
	data2.set_block_count (1);
	data2.set_cemented_count (1);
	data2.set_protocol_version (12);
	data2.set_peer_count (2);
	data2.set_bandwidth_cap (100);
	data2.set_unchecked_count (3);
	data2.set_uptime (6);
	data2.set_genesis_block (nano::block_hash (3));
	data2.set_major_version (20);
	data2.set_minor_version (1);
	data2.set_patch_version (5);
	data2.set_pre_release_version (2);
	data2.set_maker (1);
	data2.set_timestamp (std::chrono::system_clock::time_point (100ms));
	data2.set_active_difficulty (10);

	auto consolidated_telemetry_data1 = nano::consolidate_telemetry_data (all_data);
	ASSERT_EQ (consolidated_telemetry_data1.get_bandwidth_cap (), 0);

	// And a few nodes with non-zero bandwidth
	all_data.push_back (data2);
	all_data.push_back (data2);

	auto consolidated_telemetry_data2 = nano::consolidate_telemetry_data (all_data);
	ASSERT_EQ (consolidated_telemetry_data2.get_bandwidth_cap (), 0);
}

TEST (telemetry, no_peers)
{
	nano::test::system system (1);

	auto responses = system.nodes[0]->telemetry->get_metrics ();
	ASSERT_TRUE (responses.empty ());
}

TEST (telemetry, basic)
{
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_ongoing_telemetry_requests (true);
	node_flags.set_disable_initial_telemetry_requests (true);
	auto node_client = system.add_node (node_flags);
	auto node_server = system.add_node (node_flags);

	nano::test::wait_peer_connections (system);

	// Request telemetry metrics
	nano::telemetry_data telemetry_data;
	auto server_endpoint = node_server->network->endpoint ();
	auto channel = node_client->network->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	{
		std::atomic<bool> done{ false };
		node_client->telemetry->get_metrics_single_peer_async (channel, [&done, &server_endpoint, &telemetry_data] (nano::telemetry_data_response const & response_a) {
			ASSERT_FALSE (response_a.error);
			telemetry_data = response_a.telemetry_data;
			done = true;
		});

		ASSERT_TIMELY (10s, done);
		ASSERT_EQ (node_server->get_node_id (), telemetry_data.get_node_id ());
	}

	// Check the metrics are correct
	nano::test::compare_default_telemetry_response_data (telemetry_data, node_server->network_params, node_server->config->bandwidth_limit, node_server->default_difficulty (nano::work_version::work_1), node_server->node_id);

	// Call again straight away. It should use the cache
	{
		std::atomic<bool> done{ false };
		node_client->telemetry->get_metrics_single_peer_async (channel, [&done, &telemetry_data] (nano::telemetry_data_response const & response_a) {
			ASSERT_EQ (telemetry_data, response_a.telemetry_data);
			ASSERT_FALSE (response_a.error);
			done = true;
		});

		ASSERT_TIMELY (10s, done);
	}

	// Wait the cache period and check cache is not used
	std::this_thread::sleep_for (nano::telemetry_cache_cutoffs::dev ());

	std::atomic<bool> done{ false };
	node_client->telemetry->get_metrics_single_peer_async (channel, [&done, &telemetry_data] (nano::telemetry_data_response const & response_a) {
		ASSERT_NE (telemetry_data, response_a.telemetry_data);
		ASSERT_FALSE (response_a.error);
		done = true;
	});

	ASSERT_TIMELY (10s, done);
}

TEST (telemetry, receive_from_non_listening_channel)
{
	nano::test::system system;
	auto node = system.add_node ();
	nano::telemetry_ack message{ nano::dev::network_params.network, nano::telemetry_data{} };

	auto outer_node = nano::test::add_outer_node (system, nano::test::get_available_port ());
	auto channel = nano::test::establish_tcp (system, *outer_node, node->network->endpoint ());

	node->network->inbound (message, channel);
	// We have not sent a telemetry_req message to this endpoint, so shouldn't count telemetry_ack received from it.
	ASSERT_EQ (node->telemetry->telemetry_data_size (), 0);
}

TEST (telemetry, over_tcp)
{
	nano::test::system system;
	nano::node_flags node_flags;
	auto node_client = system.add_node (node_flags);
	auto node_server = system.add_node (node_flags);

	nano::test::wait_peer_connections (system);

	std::atomic<bool> done{ false };
	auto channel = node_client->network->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	node_client->telemetry->get_metrics_single_peer_async (channel, [&done, &node_server] (nano::telemetry_data_response const & response_a) {
		ASSERT_FALSE (response_a.error);
		nano::test::compare_default_telemetry_response_data (response_a.telemetry_data, node_server->network_params, node_server->config->bandwidth_limit, node_server->default_difficulty (nano::work_version::work_1), node_server->node_id);
		done = true;
	});

	ASSERT_TIMELY (10s, done);

	// Check channels are indeed tcp
	ASSERT_EQ (1, node_client->network->size ());
	auto list1 (node_client->network->list (2));
	ASSERT_EQ (node_server->network->endpoint (), list1[0]->get_endpoint ());
	ASSERT_EQ (nano::transport::transport_type::tcp, list1[0]->get_type ());
	ASSERT_EQ (1, node_server->network->size ());
	auto list2 (node_server->network->list (2));
	ASSERT_EQ (node_client->network->endpoint (), list2[0]->get_endpoint ());
	ASSERT_EQ (nano::transport::transport_type::tcp, list2[0]->get_type ());
}

TEST (telemetry, invalid_channel)
{
	nano::test::system system (2);

	auto node_client = system.nodes.front ();
	auto node_server = system.nodes.back ();

	std::atomic<bool> done{ false };
	node_client->telemetry->get_metrics_single_peer_async (nullptr, [&done] (nano::telemetry_data_response const & response_a) {
		ASSERT_TRUE (response_a.error);
		done = true;
	});

	ASSERT_TIMELY (10s, done);
}

TEST (telemetry, blocking_request)
{
	nano::test::system system (2);

	auto node_client = system.nodes.front ();
	auto node_server = system.nodes.back ();

	nano::test::wait_peer_connections (system);

	// Request telemetry metrics
	std::atomic<bool> done{ false };
	std::function<void ()> call_system_poll;
	std::promise<void> promise;
	call_system_poll = [&call_system_poll, &workers = *node_client->workers, &done, &system, &promise] () {
		if (!done)
		{
			ASSERT_NO_ERROR (system.poll ());
			workers.push_task (call_system_poll);
		}
		else
		{
			promise.set_value ();
		}
	};

	// Keep pushing system.polls in another thread (thread_pool), because we will be blocking this thread and unable to do so.
	system.deadline_set (10s);
	node_client->workers->push_task (call_system_poll);

	// Now try single request metric
	auto channel = node_client->network->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	auto telemetry_data_response = node_client->telemetry->get_metrics_single_peer (channel);
	ASSERT_FALSE (telemetry_data_response.error);
	nano::test::compare_default_telemetry_response_data (telemetry_data_response.telemetry_data, node_server->network_params, node_server->config->bandwidth_limit, node_server->default_difficulty (nano::work_version::work_1), node_server->node_id);

	done = true;
	promise.get_future ().wait ();
}

TEST (telemetry, disconnects)
{
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_initial_telemetry_requests (true);
	auto node_client = system.add_node (node_flags);
	auto node_server = system.add_node (node_flags);

	nano::test::wait_peer_connections (system);

	// Try and request metrics from a node which is turned off but a channel is not closed yet
	auto channel = node_client->network->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	node_server->stop ();
	ASSERT_TRUE (channel);

	std::atomic<bool> done{ false };
	node_client->telemetry->get_metrics_single_peer_async (channel, [&done] (nano::telemetry_data_response const & response_a) {
		ASSERT_TRUE (response_a.error);
		done = true;
	});

	ASSERT_TIMELY (10s, done);
}

TEST (telemetry, dos_tcp)
{
	// Confirm that telemetry_reqs are not processed
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_initial_telemetry_requests (true);
	node_flags.set_disable_ongoing_telemetry_requests (true);
	auto node_client = system.add_node (node_flags);
	auto node_server = system.add_node (node_flags);

	nano::test::wait_peer_connections (system);

	nano::telemetry_req message{ nano::dev::network_params.network };
	auto channel = node_client->network->tcp_channels->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	channel->send (message, [] (boost::system::error_code const & ec, size_t size_a) {
		ASSERT_FALSE (ec);
	});

	ASSERT_TIMELY (10s, 1 == node_server->stats->count (nano::stat::type::message, nano::stat::detail::telemetry_req, nano::stat::dir::in));

	auto orig = std::chrono::steady_clock::now ();
	for (int i = 0; i < 10; ++i)
	{
		channel->send (message, [] (boost::system::error_code const & ec, size_t size_a) {
			ASSERT_FALSE (ec);
		});
	}

	ASSERT_TIMELY (10s, (nano::telemetry_cache_cutoffs::dev () + orig) <= std::chrono::steady_clock::now ());

	// Should process no more telemetry_req messages
	ASSERT_EQ (1, node_server->stats->count (nano::stat::type::message, nano::stat::detail::telemetry_req, nano::stat::dir::in));

	// Now spam messages waiting for it to be processed
	while (node_server->stats->count (nano::stat::type::message, nano::stat::detail::telemetry_req, nano::stat::dir::in) == 1)
	{
		channel->send (message);
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (telemetry, disable_metrics)
{
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_initial_telemetry_requests (true);
	auto node_client = system.add_node (node_flags);
	node_flags.set_disable_providing_telemetry_metrics (true);
	auto node_server = system.add_node (node_flags);

	nano::test::wait_peer_connections (system);

	// Try and request metrics from a node which is turned off but a channel is not closed yet
	auto channel = node_client->network->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);

	std::atomic<bool> done{ false };
	node_client->telemetry->get_metrics_single_peer_async (channel, [&done] (nano::telemetry_data_response const & response_a) {
		ASSERT_TRUE (response_a.error);
		done = true;
	});

	ASSERT_TIMELY (10s, done);

	// It should still be able to receive metrics though
	done = false;
	auto channel1 = node_server->network->find_node_id (node_client->get_node_id ());
	ASSERT_NE (nullptr, channel1);
	node_server->telemetry->get_metrics_single_peer_async (channel1, [&done, node_client] (nano::telemetry_data_response const & response_a) {
		ASSERT_FALSE (response_a.error);
		nano::test::compare_default_telemetry_response_data (response_a.telemetry_data, node_client->network_params, node_client->config->bandwidth_limit, node_client->default_difficulty (nano::work_version::work_1), node_client->node_id);
		done = true;
	});

	ASSERT_TIMELY (10s, done);
}

TEST (telemetry, max_possible_size)
{
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_initial_telemetry_requests (true);
	node_flags.set_disable_ongoing_telemetry_requests (true);
	auto node_client = system.add_node (node_flags);
	auto node_server = system.add_node (node_flags);

	nano::telemetry_data data;
	std::vector<uint8_t> unknown;
	std::size_t telemetry_size_mask = 0x3ff;
	unknown.resize (telemetry_size_mask - nano::telemetry_data::latest_size ());
	data.set_unknown_data (unknown);

	nano::telemetry_ack message{ nano::dev::network_params.network, data };
	nano::test::wait_peer_connections (system);

	auto channel = node_client->network->tcp_channels->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	channel->send (message, [] (boost::system::error_code const & ec, size_t size_a) {
		ASSERT_FALSE (ec);
	});

	ASSERT_TIMELY (10s, 1 == node_server->stats->count (nano::stat::type::message, nano::stat::detail::telemetry_ack, nano::stat::dir::in));
}

namespace nano
{
// Test disabled because it's failing intermittently.
// PR in which it got disabled: https://github.com/nanocurrency/nano-node/pull/3512
// Issue for investigating it: https://github.com/nanocurrency/nano-node/issues/3524
TEST (telemetry, DISABLED_remove_peer_different_genesis)
{
	nano::test::system system (1);
	auto node0 (system.nodes[0]);
	ASSERT_EQ (0, node0->network->size ());
	// Change genesis block to something else in this test (this is the reference telemetry processing uses).
	nano::network_params network_params{ nano::networks::nano_dev_network };
	network_params.ledger.genesis = network_params.ledger.nano_live_genesis;
	nano::node_config config{ network_params };
	auto node1 (std::make_shared<nano::node> (system.io_ctx, nano::unique_path (), config, system.work));
	node1->start ();
	system.nodes.push_back (node1);
	node0->network->merge_peer (node1->network->endpoint ());
	node1->network->merge_peer (node0->network->endpoint ());
	ASSERT_TIMELY (10s, node0->stats->count (nano::stat::type::telemetry, nano::stat::detail::different_genesis_hash) != 0 && node1->stats->count (nano::stat::type::telemetry, nano::stat::detail::different_genesis_hash) != 0);

	ASSERT_TIMELY (1s, 0 == node0->network->size ());
	ASSERT_TIMELY (1s, 0 == node1->network->size ());
	ASSERT_GE (node0->stats->count (nano::stat::type::message, nano::stat::detail::node_id_handshake, nano::stat::dir::out), 1);
	ASSERT_GE (node1->stats->count (nano::stat::type::message, nano::stat::detail::node_id_handshake, nano::stat::dir::out), 1);

	ASSERT_TRUE (node0->network->excluded_peers.contains (nano::transport::map_endpoint_to_tcp (node1->network->endpoint ())));
	ASSERT_TRUE (node1->network->excluded_peers.contains (nano::transport::map_endpoint_to_tcp (node0->network->endpoint ())));
}

TEST (telemetry, remove_peer_invalid_signature)
{
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_initial_telemetry_requests (true);
	node_flags.set_disable_ongoing_telemetry_requests (true);
	auto node = system.add_node (node_flags);
	auto outer_node = nano::test::add_outer_node (system, nano::test::get_available_port ());
	auto channel = nano::test::establish_tcp (system, *outer_node, node->network->endpoint ());
	channel->set_node_id (node->node_id.pub);
	// (Implementation detail) So that messages are not just discarded when requests were not sent.
	node->telemetry->recent_or_initial_request_telemetry_data.emplace (channel->get_endpoint (), nano::telemetry_data (), std::chrono::steady_clock::now (), true);

	auto telemetry_data = nano::local_telemetry_data (node->ledger, *node->network, node->unchecked, node->config->bandwidth_limit, node->network_params, node->startup_time, node->default_difficulty (nano::work_version::work_1), node->node_id);
	// Change anything so that the signed message is incorrect
	telemetry_data.set_block_count (0);
	auto telemetry_ack = nano::telemetry_ack{ nano::dev::network_params.network, telemetry_data };
	node->network->inbound (telemetry_ack, channel);

	ASSERT_TIMELY (10s, node->stats->count (nano::stat::type::telemetry, nano::stat::detail::invalid_signature) > 0);
	ASSERT_NO_ERROR (system.poll_until_true (3s, [&node, endpoint = channel->get_endpoint ()] () -> bool {
		return node->network->excluded_peers.contains (nano::transport::map_endpoint_to_tcp (endpoint));
	}));
}
}

TEST (telemetry, maker_pruning)
{
	nano::test::system system;
	nano::node_flags node_flags;
	node_flags.set_disable_ongoing_telemetry_requests (true);
	node_flags.set_disable_initial_telemetry_requests (true);
	auto node_client = system.add_node (node_flags);
	node_flags.set_enable_pruning (true);
	nano::node_config config;
	config.enable_voting = false;
	auto node_server = system.add_node (config, node_flags);

	nano::test::wait_peer_connections (system);

	// Request telemetry metrics
	nano::telemetry_data telemetry_data;
	auto server_endpoint = node_server->network->endpoint ();
	auto channel = node_client->network->find_node_id (node_server->get_node_id ());
	ASSERT_NE (nullptr, channel);
	{
		std::atomic<bool> done{ false };
		node_client->telemetry->get_metrics_single_peer_async (channel, [&done, &server_endpoint, &telemetry_data] (nano::telemetry_data_response const & response_a) {
			ASSERT_FALSE (response_a.error);
			telemetry_data = response_a.telemetry_data;
			done = true;
		});

		ASSERT_TIMELY (10s, done);
		ASSERT_EQ (node_server->get_node_id (), telemetry_data.get_node_id ());
	}

	ASSERT_EQ (nano::telemetry_maker::nf_pruned_node, static_cast<nano::telemetry_maker> (telemetry_data.get_maker ()));
}
