#include <nano/node/bootstrap/bootstrap_attempt.hpp>
#include <nano/node/bootstrap/bootstrap_frontier.hpp>
#include <nano/node/bootstrap/bootstrap_legacy.hpp>
#include <nano/node/node.hpp>
#include <nano/node/transport/tcp.hpp>

#include <boost/format.hpp>

constexpr double nano::bootstrap_limits::bootstrap_connection_warmup_time_sec;
constexpr double nano::bootstrap_limits::bootstrap_minimum_elapsed_seconds_blockrate;
constexpr double nano::bootstrap_limits::bootstrap_minimum_frontier_blocks_per_sec;
constexpr unsigned nano::bootstrap_limits::bulk_push_cost_limit;

constexpr std::size_t nano::frontier_req_client::size_frontier;

void nano::frontier_req_client::run (nano::account const & start_account_a, uint32_t const frontiers_age_a, uint32_t const count_a)
{
	nano::frontier_req request{ node->network_params.network };
	request.set_start ((start_account_a.is_zero () || start_account_a.number () == std::numeric_limits<nano::uint256_t>::max ()) ? start_account_a : start_account_a.number () + 1);
	request.set_age (frontiers_age_a);
	request.set_count (count_a);
	current = start_account_a;
	frontiers_age = frontiers_age_a;
	count_limit = count_a;
	next (); // Load accounts from disk
	auto this_l (shared_from_this ());
	connection->send (
	request, [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		if (!ec)
		{
			this_l->receive_frontier ();
		}
		else
		{
			if (this_l->node->config->logging.network_logging ())
			{
				this_l->node->logger->try_log (boost::str (boost::format ("Error while sending bootstrap request %1%") % ec.message ()));
			}
		}
	},
	nano::buffer_drop_policy::no_limiter_drop);
}

nano::frontier_req_client::frontier_req_client (std::shared_ptr<nano::node> const & node_a, std::shared_ptr<nano::bootstrap_client> const & connection_a, std::shared_ptr<nano::bootstrap_attempt_legacy> const & attempt_a) :
	node (node_a),
	connection (connection_a),
	attempt (attempt_a),
	count (0),
	bulk_push_cost (0)
{
}

void nano::frontier_req_client::receive_frontier ()
{
	auto this_l (shared_from_this ());
	connection->async_read (nano::frontier_req_client::size_frontier, [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		// An issue with asio is that sometimes, instead of reporting a bad file descriptor during disconnect,
		// we simply get a size of 0.
		if (size_a == nano::frontier_req_client::size_frontier)
		{
			this_l->received_frontier (ec, size_a);
		}
		else
		{
			if (this_l->node->config->logging.network_message_logging ())
			{
				this_l->node->logger->try_log (boost::str (boost::format ("Invalid size: expected %1%, got %2%") % nano::frontier_req_client::size_frontier % size_a));
			}
		}
	});
}

bool nano::frontier_req_client::bulk_push_available ()
{
	return bulk_push_cost < nano::bootstrap_limits::bulk_push_cost_limit && frontiers_age == std::numeric_limits<decltype (frontiers_age)>::max ();
}

void nano::frontier_req_client::unsynced (nano::block_hash const & head, nano::block_hash const & end)
{
	if (bulk_push_available ())
	{
		attempt->add_bulk_push_target (head, end);
		if (end.is_zero ())
		{
			bulk_push_cost += 2;
		}
		else
		{
			bulk_push_cost += 1;
		}
	}
}

void nano::frontier_req_client::received_frontier (boost::system::error_code const & ec, std::size_t size_a)
{
	if (!ec)
	{
		debug_assert (size_a == nano::frontier_req_client::size_frontier);
		nano::account account;
		nano::bufferstream account_stream (connection->get_receive_buffer (), sizeof (account));
		auto error1 (nano::try_read (account_stream, account));
		(void)error1;
		debug_assert (!error1);
		nano::block_hash latest;
		nano::bufferstream latest_stream (connection->get_receive_buffer () + sizeof (account), sizeof (latest));
		auto error2 (nano::try_read (latest_stream, latest));
		(void)error2;
		debug_assert (!error2);
		if (count == 0)
		{
			start_time = std::chrono::steady_clock::now ();
		}
		++count;
		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>> (std::chrono::steady_clock::now () - start_time);

		double elapsed_sec = std::max (time_span.count (), nano::bootstrap_limits::bootstrap_minimum_elapsed_seconds_blockrate);
		double blocks_per_sec = static_cast<double> (count) / elapsed_sec;
		double age_factor = (frontiers_age == std::numeric_limits<decltype (frontiers_age)>::max ()) ? 1.0 : 1.5; // Allow slower frontiers receive for requests with age
		if (elapsed_sec > nano::bootstrap_limits::bootstrap_connection_warmup_time_sec && blocks_per_sec * age_factor < nano::bootstrap_limits::bootstrap_minimum_frontier_blocks_per_sec)
		{
			node->logger->try_log (boost::str (boost::format ("Aborting frontier req because it was too slow: %1% frontiers per second, last %2%") % blocks_per_sec % account.to_account ()));
			promise.set_value (true);
			return;
		}
		if (attempt->should_log ())
		{
			node->logger->always_log (boost::str (boost::format ("Received %1% frontiers from %2%") % std::to_string (count) % connection->channel_string ()));
		}
		if (!account.is_zero () && count <= count_limit)
		{
			last_account = account;
			while (!current.is_zero () && current < account)
			{
				// We know about an account they don't.
				unsynced (frontier, 0);
				next ();
			}
			if (!current.is_zero ())
			{
				if (account == current)
				{
					if (latest == frontier)
					{
						// In sync
					}
					else
					{
						if (node->ledger.block_or_pruned_exists (latest))
						{
							// We know about a block they don't.
							unsynced (frontier, latest);
						}
						else
						{
							attempt->add_frontier (nano::pull_info (account, latest, frontier, attempt->get_incremental_id (), 0, node->network_params.bootstrap.frontier_retry_limit));
							// Either we're behind or there's a fork we differ on
							// Either way, bulk pushing will probably not be effective
							bulk_push_cost += 5;
						}
					}
					next ();
				}
				else
				{
					debug_assert (account < current);
					attempt->add_frontier (nano::pull_info (account, latest, nano::block_hash (0), attempt->get_incremental_id (), 0, node->network_params.bootstrap.frontier_retry_limit));
				}
			}
			else
			{
				attempt->add_frontier (nano::pull_info (account, latest, nano::block_hash (0), attempt->get_incremental_id (), 0, node->network_params.bootstrap.frontier_retry_limit));
			}
			receive_frontier ();
		}
		else
		{
			if (count <= count_limit)
			{
				while (!current.is_zero () && bulk_push_available ())
				{
					// We know about an account they don't.
					unsynced (frontier, 0);
					next ();
				}
				// Prevent new frontier_req requests
				attempt->set_start_account (std::numeric_limits<nano::uint256_t>::max ());
				if (node->config->logging.bulk_pull_logging ())
				{
					node->logger->try_log ("Bulk push cost: ", bulk_push_cost);
				}
			}
			else
			{
				// Set last processed account as new start target
				attempt->set_start_account (last_account);
			}
			node->bootstrap_initiator.connections->pool_connection (connection);
			try
			{
				promise.set_value (false);
			}
			catch (std::future_error &)
			{
			}
		}
	}
	else
	{
		if (node->config->logging.network_logging ())
		{
			node->logger->try_log (boost::str (boost::format ("Error while receiving frontier %1%") % ec.message ()));
		}
	}
}

void nano::frontier_req_client::next ()
{
	// Filling accounts deque to prevent often read transactions
	if (accounts.empty ())
	{
		std::size_t max_size (128);
		auto transaction (node->store.tx_begin_read ());
		for (auto i (node->store.account ().begin (*transaction, current.number () + 1)), n (node->store.account ().end ()); i != n && accounts.size () != max_size; ++i)
		{
			nano::account_info const & info (i->second);
			nano::account const & account (i->first);
			accounts.emplace_back (account, info.head ());
		}

		/* If loop breaks before max_size, then accounts_end () is reached. Add empty record */
		if (accounts.size () != max_size)
		{
			accounts.emplace_back (nano::account{}, nano::block_hash (0));
		}
	}
	// Retrieving accounts from deque
	auto const & account_pair (accounts.front ());
	current = account_pair.first;
	frontier = account_pair.second;
	accounts.pop_front ();
}

nano::frontier_req_server::frontier_req_server (std::shared_ptr<nano::node> const & node_a, std::shared_ptr<nano::transport::tcp_server> const & connection_a, std::unique_ptr<nano::frontier_req> request_a) :
	node (node_a),
	connection (connection_a),
	current (request_a->get_start ().number () - 1),
	frontier (0),
	request (std::move (request_a)),
	count (0)
{
	next ();
}

void nano::frontier_req_server::send_next ()
{
	if (!current.is_zero () && count < request->get_count ())
	{
		std::vector<uint8_t> send_buffer;
		{
			nano::vectorstream stream (send_buffer);
			write (stream, current.bytes);
			write (stream, frontier.bytes);
			debug_assert (!current.is_zero ());
			debug_assert (!frontier.is_zero ());
		}
		auto this_l (shared_from_this ());
		if (node->config->logging.bulk_pull_logging ())
		{
			node->logger->try_log (boost::str (boost::format ("Sending frontier for %1% %2%") % current.to_account () % frontier.to_string ()));
		}
		next ();
		connection->get_socket ()->async_write (nano::shared_const_buffer (std::move (send_buffer)), [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
			this_l->sent_action (ec, size_a);
		});
	}
	else
	{
		send_finished ();
	}
}

void nano::frontier_req_server::send_finished ()
{
	std::vector<uint8_t> send_buffer;
	{
		nano::vectorstream stream (send_buffer);
		nano::uint256_union zero (0);
		write (stream, zero.bytes);
		write (stream, zero.bytes);
	}
	auto this_l (shared_from_this ());
	if (node->config->logging.network_logging ())
	{
		node->logger->try_log ("Frontier sending finished");
	}
	connection->get_socket ()->async_write (nano::shared_const_buffer (std::move (send_buffer)), [this_l] (boost::system::error_code const & ec, std::size_t size_a) {
		this_l->no_block_sent (ec, size_a);
	});
}

void nano::frontier_req_server::no_block_sent (boost::system::error_code const & ec, std::size_t size_a)
{
	if (!ec)
	{
		connection->start ();
	}
	else
	{
		if (node->config->logging.network_logging ())
		{
			node->logger->try_log (boost::str (boost::format ("Error sending frontier finish: %1%") % ec.message ()));
		}
	}
}

void nano::frontier_req_server::sent_action (boost::system::error_code const & ec, std::size_t size_a)
{
	if (!ec)
	{
		count++;

		node->bootstrap_workers.push_task ([this_l = shared_from_this ()] () {
			this_l->send_next ();
		});
	}
	else
	{
		if (node->config->logging.network_logging ())
		{
			node->logger->try_log (boost::str (boost::format ("Error sending frontier pair: %1%") % ec.message ()));
		}
	}
}

void nano::frontier_req_server::next ()
{
	// Filling accounts deque to prevent often read transactions
	if (accounts.empty ())
	{
		auto now (nano::seconds_since_epoch ());
		bool disable_age_filter (request->get_age () == std::numeric_limits<decltype (request->get_age ())>::max ());
		std::size_t max_size (128);
		auto transaction (node->store.tx_begin_read ());
		if (!send_confirmed ())
		{
			for (auto i (node->store.account ().begin (*transaction, current.number () + 1)), n (node->store.account ().end ()); i != n && accounts.size () != max_size; ++i)
			{
				nano::account_info const & info (i->second);
				if (disable_age_filter || (now - info.modified ()) <= request->get_age ())
				{
					nano::account const & account (i->first);
					accounts.emplace_back (account, info.head ());
				}
			}
		}
		else
		{
			for (auto i (node->store.confirmation_height ().begin (*transaction, current.number () + 1)), n (node->store.confirmation_height ().end ()); i != n && accounts.size () != max_size; ++i)
			{
				nano::confirmation_height_info const & info (i->second);
				nano::block_hash const confirmed_frontier (info.frontier ());
				if (!confirmed_frontier.is_zero ())
				{
					nano::account const & account (i->first);
					accounts.emplace_back (account, confirmed_frontier);
				}
			}
		}

		/* If loop breaks before max_size, then accounts_end () is reached. Add empty record to finish frontier_req_server */
		if (accounts.size () != max_size)
		{
			accounts.emplace_back (nano::account{}, nano::block_hash (0));
		}
	}
	// Retrieving accounts from deque
	auto const & account_pair (accounts.front ());
	current = account_pair.first;
	frontier = account_pair.second;
	accounts.pop_front ();
}

bool nano::frontier_req_server::send_confirmed ()
{
	return request->is_only_confirmed_present ();
}
