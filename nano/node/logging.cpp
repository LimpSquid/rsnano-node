#include <nano/lib/config.hpp>
#include <nano/lib/jsonconfig.hpp>
#include <nano/lib/logger_mt.hpp>
#include <nano/lib/tomlconfig.hpp>
#include <nano/node/logging.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

#ifdef BOOST_WINDOWS
#include <boost/log/sinks/event_log_backend.hpp>
#else
#define BOOST_LOG_USE_NATIVE_SYSLOG
#include <boost/log/sinks/syslog_backend.hpp>
#endif

BOOST_LOG_ATTRIBUTE_KEYWORD (severity, "Severity", nano::severity_level)

boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>> nano::logging::file_sink;
std::atomic_flag nano::logging::logging_already_added ATOMIC_FLAG_INIT;

nano::logging::logging ()
{
	rsnano::LoggingDto dto;
	rsnano::rsn_logging_create (&dto);
	ledger_logging_value = dto.ledger_logging_value;
	ledger_duplicate_logging_value = dto.ledger_duplicate_logging_value;
	ledger_rollback_logging_value = dto.ledger_rollback_logging_value;
	vote_logging_value = dto.vote_logging_value;
	rep_crawler_logging_value = dto.rep_crawler_logging_value;
	election_fork_tally_logging_value = dto.election_fork_tally_logging_value;
	election_expiration_tally_logging_value = dto.election_expiration_tally_logging_value;
	network_logging_value = dto.network_logging_value;
	network_timeout_logging_value = dto.network_timeout_logging_value;
	network_message_logging_value = dto.network_message_logging_value;
	network_publish_logging_value = dto.network_publish_logging_value;
	network_packet_logging_value = dto.network_packet_logging_value;
	network_keepalive_logging_value = dto.network_keepalive_logging_value;
	network_node_id_handshake_logging_value = dto.network_node_id_handshake_logging_value;
	network_telemetry_logging_value = dto.network_telemetry_logging_value;
	network_rejected_logging_value = dto.network_rejected_logging_value;
	node_lifetime_tracing_value = dto.node_lifetime_tracing_value;
	insufficient_work_logging_value = dto.insufficient_work_logging_value;
	log_ipc_value = dto.log_ipc_value;
	bulk_pull_logging_value = dto.bulk_pull_logging_value;
	work_generation_time_value = dto.work_generation_time_value;
	upnp_details_logging_value = dto.upnp_details_logging_value;
	timing_logging_value = dto.timing_logging_value;
	active_update_value = dto.active_update_value;
	log_to_cerr_value = dto.log_to_cerr_value;
	flush = dto.flush;
	max_size = dto.max_size;
	rotation_size = dto.rotation_size;
	stable_log_filename = dto.stable_log_filename;
	min_time_between_log_output = std::chrono::milliseconds (dto.min_time_between_log_output_ms);
	single_line_record_value = dto.single_line_record_value;
	election_result_logging_value = dto.election_result_logging_value;
}

rsnano::LoggingDto nano::logging::to_dto () const
{
	rsnano::LoggingDto dto;
	dto.ledger_logging_value = ledger_logging_value;
	dto.ledger_duplicate_logging_value = ledger_duplicate_logging_value;
	dto.ledger_rollback_logging_value = ledger_rollback_logging_value;
	dto.vote_logging_value = vote_logging_value;
	dto.rep_crawler_logging_value = rep_crawler_logging_value;
	dto.election_fork_tally_logging_value = election_fork_tally_logging_value;
	dto.election_expiration_tally_logging_value = election_expiration_tally_logging_value;
	dto.network_logging_value = network_logging_value;
	dto.network_timeout_logging_value = network_timeout_logging_value;
	dto.network_message_logging_value = network_message_logging_value;
	dto.network_publish_logging_value = network_publish_logging_value;
	dto.network_packet_logging_value = network_packet_logging_value;
	dto.network_keepalive_logging_value = network_keepalive_logging_value;
	dto.network_node_id_handshake_logging_value = network_node_id_handshake_logging_value;
	dto.network_telemetry_logging_value = network_telemetry_logging_value;
	dto.network_rejected_logging_value = network_rejected_logging_value;
	dto.node_lifetime_tracing_value = node_lifetime_tracing_value;
	dto.insufficient_work_logging_value = insufficient_work_logging_value;
	dto.log_ipc_value = log_ipc_value;
	dto.bulk_pull_logging_value = bulk_pull_logging_value;
	dto.work_generation_time_value = work_generation_time_value;
	dto.upnp_details_logging_value = upnp_details_logging_value;
	dto.timing_logging_value = timing_logging_value;
	dto.active_update_value = active_update_value;
	dto.log_to_cerr_value = log_to_cerr_value;
	dto.flush = flush;
	dto.max_size = max_size;
	dto.rotation_size = rotation_size;
	dto.stable_log_filename = stable_log_filename;
	dto.min_time_between_log_output_ms = min_time_between_log_output.count ();
	dto.single_line_record_value = single_line_record_value;
	dto.election_result_logging_value = election_fork_tally_logging_value;
	return dto;
}

void nano::logging::init (boost::filesystem::path const & application_path_a)
{
	if (!logging_already_added.test_and_set ())
	{
		boost::log::add_common_attributes ();
		auto format = boost::log::expressions::stream << boost::log::expressions::attr<severity_level, severity_tag> ("Severity") << boost::log::expressions::smessage;
		auto format_with_timestamp = boost::log::expressions::stream << "[" << boost::log::expressions::attr<boost::posix_time::ptime> ("TimeStamp") << "]: " << boost::log::expressions::attr<severity_level, severity_tag> ("Severity") << boost::log::expressions::smessage;

		if (log_to_cerr ())
		{
			boost::log::add_console_log (std::cerr, boost::log::keywords::format = format_with_timestamp);
		}

#ifdef BOOST_WINDOWS
		if (nano::event_log_reg_entry_exists () || nano::is_windows_elevated ())
		{
			static auto event_sink = boost::make_shared<boost::log::sinks::synchronous_sink<boost::log::sinks::simple_event_log_backend>> (boost::log::keywords::log_name = "Nano", boost::log::keywords::log_source = "Nano");
			event_sink->set_formatter (format);

			// Currently only mapping sys log errors
			boost::log::sinks::event_log::custom_event_type_mapping<nano::severity_level> mapping ("Severity");
			mapping[nano::severity_level::error] = boost::log::sinks::event_log::error;
			event_sink->locked_backend ()->set_event_type_mapper (mapping);

			// Only allow messages or error or greater severity to the event log
			event_sink->set_filter (severity >= nano::severity_level::error);
			boost::log::core::get ()->add_sink (event_sink);
		}
#else
		static auto sys_sink = boost::make_shared<boost::log::sinks::synchronous_sink<boost::log::sinks::syslog_backend>> (boost::log::keywords::facility = boost::log::sinks::syslog::user, boost::log::keywords::use_impl = boost::log::sinks::syslog::impl_types::native);
		sys_sink->set_formatter (format);

		// Currently only mapping sys log errors
		boost::log::sinks::syslog::custom_severity_mapping<nano::severity_level> mapping ("Severity");
		mapping[nano::severity_level::error] = boost::log::sinks::syslog::error;
		sys_sink->locked_backend ()->set_severity_mapper (mapping);

		// Only allow messages or error or greater severity to the sys log
		sys_sink->set_filter (severity >= nano::severity_level::error);
		boost::log::core::get ()->add_sink (sys_sink);
#endif

//clang-format off
#if BOOST_VERSION < 107000
		if (stable_log_filename)
		{
			stable_log_filename = false;
			std::cerr << "The stable_log_filename config setting is only available when building with Boost 1.70 or later. Reverting to old behavior." << std::endl;
		}
#endif

		auto path = application_path_a / "log";
		if (stable_log_filename)
		{
#if BOOST_VERSION >= 107000
			auto const file_name = path / "node.log";
			// Logging to node.log and node_<pattern> instead of log_<pattern>.log is deliberate. This way,
			// existing log monitoring scripts expecting the old logfile structure will fail immediately instead
			// of reading only rotated files with old entries.
			file_sink = boost::log::add_file_log (boost::log::keywords::target = path,
			boost::log::keywords::file_name = file_name,
			boost::log::keywords::target_file_name = path / "node_%Y-%m-%d_%H-%M-%S.%N.log",
			boost::log::keywords::open_mode = std::ios_base::out | std::ios_base::app, // append to node.log if it exists
			boost::log::keywords::enable_final_rotation = false, // for stable log filenames, don't rotate on destruction
			boost::log::keywords::rotation_size = rotation_size, // max file size in bytes before rotation
			boost::log::keywords::auto_flush = flush,
			boost::log::keywords::scan_method = boost::log::sinks::file::scan_method::scan_matching,
			boost::log::keywords::max_size = max_size, // max total size in bytes of all log files
			boost::log::keywords::format = format_with_timestamp);

			if (!boost::filesystem::exists (file_name))
			{
				// Create temp stream to first create the file
				std::ofstream stream (file_name.string ());
			}

			// Set permissions before opening otherwise Windows only has read permissions
			nano::set_secure_perm_file (file_name);

#else
			debug_assert (false);
#endif
		}
		else
		{
			file_sink = boost::log::add_file_log (boost::log::keywords::target = path,
			boost::log::keywords::file_name = path / "log_%Y-%m-%d_%H-%M-%S.%N.log",
			boost::log::keywords::rotation_size = rotation_size,
			boost::log::keywords::auto_flush = flush,
			boost::log::keywords::scan_method = boost::log::sinks::file::scan_method::scan_matching,
			boost::log::keywords::max_size = max_size,
			boost::log::keywords::format = format_with_timestamp);
		}

		struct exception_handler
		{
			void operator() (std::exception const & e) const
			{
				std::cerr << "Logging exception: " << e.what () << std::endl;
			}
		};

		boost::log::core::get ()->set_exception_handler (boost::log::make_exception_handler<std::exception> (exception_handler ()));
	}
	//clang-format on
}

void nano::logging::release_file_sink ()
{
	if (logging_already_added.test_and_set ())
	{
		boost::log::core::get ()->remove_sink (nano::logging::file_sink);
		nano::logging::file_sink.reset ();
		logging_already_added.clear ();
	}
}

nano::error nano::logging::deserialize_toml (nano::tomlconfig & toml)
{
	toml.get<bool> ("ledger", ledger_logging_value);
	toml.get<bool> ("ledger_duplicate", ledger_duplicate_logging_value);
	toml.get<bool> ("ledger_rollback", ledger_rollback_logging_value);
	toml.get<bool> ("vote", vote_logging_value);
	toml.get<bool> ("rep_crawler", rep_crawler_logging_value);
	toml.get<bool> ("election_expiration", election_expiration_tally_logging_value);
	toml.get<bool> ("election_fork", election_fork_tally_logging_value);
	toml.get<bool> ("network", network_logging_value);
	toml.get<bool> ("network_timeout", network_timeout_logging_value);
	toml.get<bool> ("network_message", network_message_logging_value);
	toml.get<bool> ("network_publish", network_publish_logging_value);
	toml.get<bool> ("network_packet", network_packet_logging_value);
	toml.get<bool> ("network_keepalive", network_keepalive_logging_value);
	toml.get<bool> ("network_node_id_handshake", network_node_id_handshake_logging_value);
	toml.get<bool> ("network_telemetry_logging", network_telemetry_logging_value);
	toml.get<bool> ("network_rejected_logging", network_rejected_logging_value);
	toml.get<bool> ("node_lifetime_tracing", node_lifetime_tracing_value);
	toml.get<bool> ("insufficient_work", insufficient_work_logging_value);
	toml.get<bool> ("log_ipc", log_ipc_value);
	toml.get<bool> ("bulk_pull", bulk_pull_logging_value);
	toml.get<bool> ("work_generation_time", work_generation_time_value);
	toml.get<bool> ("upnp_details", upnp_details_logging_value);
	toml.get<bool> ("timing", timing_logging_value);
	toml.get<bool> ("active_update", active_update_value);
	toml.get<bool> ("election_result", election_result_logging_value);
	toml.get<bool> ("log_to_cerr", log_to_cerr_value);
	toml.get<bool> ("flush", flush);
	toml.get<bool> ("single_line_record", single_line_record_value);
	toml.get<uintmax_t> ("max_size", max_size);
	toml.get<uintmax_t> ("rotation_size", rotation_size);
	auto min_time_between_log_output_l = min_time_between_log_output.count ();
	toml.get ("min_time_between_output", min_time_between_log_output_l);
	min_time_between_log_output = std::chrono::milliseconds (min_time_between_log_output_l);
	toml.get ("stable_log_filename", stable_log_filename);

	return toml.get_error ();
}

bool nano::logging::ledger_logging () const
{
	return ledger_logging_value;
}

bool nano::logging::ledger_duplicate_logging () const
{
	return ledger_logging () && ledger_duplicate_logging_value;
}

bool nano::logging::ledger_rollback_logging () const
{
	return ledger_rollback_logging_value;
}

bool nano::logging::vote_logging () const
{
	return vote_logging_value;
}

bool nano::logging::rep_crawler_logging () const
{
	return rep_crawler_logging_value;
}

bool nano::logging::election_expiration_tally_logging () const
{
	return election_expiration_tally_logging_value;
}

bool nano::logging::election_fork_tally_logging () const
{
	return election_fork_tally_logging_value;
}

bool nano::logging::network_logging () const
{
	return network_logging_value;
}

bool nano::logging::network_timeout_logging () const
{
	return network_logging () && network_timeout_logging_value;
}

bool nano::logging::network_message_logging () const
{
	return network_logging () && network_message_logging_value;
}

bool nano::logging::network_publish_logging () const
{
	return network_logging () && network_publish_logging_value;
}

bool nano::logging::network_packet_logging () const
{
	return network_logging () && network_packet_logging_value;
}

bool nano::logging::network_keepalive_logging () const
{
	return network_logging () && network_keepalive_logging_value;
}

bool nano::logging::network_node_id_handshake_logging () const
{
	return network_logging () && network_node_id_handshake_logging_value;
}

bool nano::logging::network_telemetry_logging () const
{
	return network_logging () && network_telemetry_logging_value;
}

bool nano::logging::network_rejected_logging () const
{
	return network_logging () && network_rejected_logging_value;
}

bool nano::logging::node_lifetime_tracing () const
{
	return node_lifetime_tracing_value;
}

bool nano::logging::insufficient_work_logging () const
{
	return network_logging () && insufficient_work_logging_value;
}

bool nano::logging::log_ipc () const
{
	return network_logging () && log_ipc_value;
}

bool nano::logging::bulk_pull_logging () const
{
	return network_logging () && bulk_pull_logging_value;
}

bool nano::logging::callback_logging () const
{
	return network_logging ();
}

bool nano::logging::work_generation_time () const
{
	return work_generation_time_value;
}

bool nano::logging::upnp_details_logging () const
{
	return upnp_details_logging_value;
}

bool nano::logging::timing_logging () const
{
	return timing_logging_value;
}

bool nano::logging::active_update_logging () const
{
	return active_update_value;
}

bool nano::logging::election_result_logging () const
{
	return election_result_logging_value;
}

bool nano::logging::log_to_cerr () const
{
	return log_to_cerr_value;
}

bool nano::logging::single_line_record () const
{
	return single_line_record_value;
}
