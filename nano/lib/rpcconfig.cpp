#include <nano/boost/asio/ip/address_v6.hpp>
#include <nano/lib/config.hpp>
#include <nano/lib/rpcconfig.hpp>
#include <nano/lib/tomlconfig.hpp>

#include <boost/dll/runtime_symbol_info.hpp>

nano::error nano::rpc_secure_config::serialize_toml (nano::tomlconfig & toml) const
{
	toml.put ("enable", enable, "Enable or disable TLS support.\ntype:bool");
	toml.put ("verbose_logging", verbose_logging, "Enable or disable verbose logging.\ntype:bool");
	toml.put ("server_key_passphrase", server_key_passphrase, "Server key passphrase.\ntype:string");
	toml.put ("server_cert_path", server_cert_path, "Directory containing certificates.\ntype:string,path");
	toml.put ("server_key_path", server_key_path, "Path to server key PEM file.\ntype:string,path");
	toml.put ("server_dh_path", server_dh_path, "Path to Diffie-Hellman params file.\ntype:string,path");
	toml.put ("client_certs_path", client_certs_path, "Directory containing client certificates.\ntype:string");
	return toml.get_error ();
}

nano::error nano::rpc_secure_config::deserialize_toml (nano::tomlconfig & toml)
{
	toml.get<bool> ("enable", enable);
	toml.get<bool> ("verbose_logging", verbose_logging);
	toml.get<std::string> ("server_key_passphrase", server_key_passphrase);
	toml.get<std::string> ("server_cert_path", server_cert_path);
	toml.get<std::string> ("server_key_path", server_key_path);
	toml.get<std::string> ("server_dh_path", server_dh_path);
	toml.get<std::string> ("client_certs_path", client_certs_path);
	return toml.get_error ();
}

nano::rpc_config::rpc_config (nano::network_constants & network_constants) :
	rpc_process{ network_constants }
{
	rsnano::RpcConfigDto dto;
	auto network_dto{ network_constants.to_dto () };
	if (rsnano::rsn_rpc_config_create (&dto, &network_dto) < 0)
		throw std::runtime_error ("could not create rpc_config");
	load_dto (dto);
}

nano::rpc_config::rpc_config (nano::network_constants & network_constants, uint16_t port_a, bool enable_control_a) :
	rpc_process{ network_constants }
{
	rsnano::RpcConfigDto dto;
	auto network_dto{ network_constants.to_dto () };
	if (rsnano::rsn_rpc_config_create2 (&dto, &network_dto, port_a, enable_control_a) < 0)
		throw std::runtime_error ("could not create rpc_config");
	load_dto (dto);
}

void nano::rpc_config::load_dto (rsnano::RpcConfigDto & dto)
{
	address = std::string (reinterpret_cast<const char *> (dto.address), dto.address_len);
	port = dto.port;
	enable_control = dto.enable_control;
	max_json_depth = dto.max_json_depth;
	max_request_size = dto.max_request_size;
	rpc_logging.log_rpc = dto.rpc_log;
	rpc_process.io_threads = dto.rpc_process.io_threads;
	rpc_process.ipc_address = std::string (reinterpret_cast<const char *> (dto.rpc_process.ipc_address), dto.rpc_process.ipc_address_len);
	rpc_process.ipc_port = dto.rpc_process.ipc_port;
	rpc_process.num_ipc_connections = dto.rpc_process.num_ipc_connections;
}

rsnano::RpcConfigDto nano::rpc_config::to_dto () const
{
	rsnano::RpcConfigDto dto;
	std::copy (address.begin (), address.end (), std::begin (dto.address));
	dto.address_len = address.size ();
	dto.port = port;
	dto.enable_control = enable_control;
	dto.max_json_depth = max_json_depth;
	dto.max_request_size = max_request_size;
	dto.rpc_log = rpc_logging.log_rpc;
	dto.rpc_process.io_threads = rpc_process.io_threads;
	std::copy (rpc_process.ipc_address.begin (), rpc_process.ipc_address.end (), std::begin (dto.rpc_process.ipc_address));
	dto.rpc_process.ipc_address_len = rpc_process.ipc_address.size ();
	dto.rpc_process.ipc_port = rpc_process.ipc_port;
	dto.rpc_process.num_ipc_connections = rpc_process.num_ipc_connections;
	return dto;
}

nano::error nano::rpc_config::serialize_toml (nano::tomlconfig & toml) const
{
	auto dto{ to_dto () };
	if (rsnano::rsn_rpc_config_serialize_toml (&dto, &toml) < 0)
		return nano::error ("could not TOML serialize rpc_config");

	return toml.get_error ();
}

nano::error nano::rpc_config::deserialize_toml (nano::tomlconfig & toml)
{
	if (!toml.empty ())
	{
		auto rpc_secure_l (toml.get_optional_child ("secure"));
		if (rpc_secure_l)
		{
			return nano::error ("The RPC secure configuration has moved to config-tls.toml. Please update the configuration.");
		}

		boost::asio::ip::address_v6 address_l;
		toml.get_optional<boost::asio::ip::address_v6> ("address", address_l, boost::asio::ip::address_v6::loopback ());
		address = address_l.to_string ();
		toml.get_optional<uint16_t> ("port", port);
		toml.get_optional<bool> ("enable_control", enable_control);
		toml.get_optional<uint8_t> ("max_json_depth", max_json_depth);
		toml.get_optional<uint64_t> ("max_request_size", max_request_size);

		auto rpc_logging_l (toml.get_optional_child ("logging"));
		if (rpc_logging_l)
		{
			rpc_logging_l->get_optional<bool> ("log_rpc", rpc_logging.log_rpc);
		}

		auto rpc_process_l (toml.get_optional_child ("process"));
		if (rpc_process_l)
		{
			rpc_process_l->get_optional<unsigned> ("io_threads", rpc_process.io_threads);
			rpc_process_l->get_optional<uint16_t> ("ipc_port", rpc_process.ipc_port);
			boost::asio::ip::address_v6 ipc_address_l;
			rpc_process_l->get_optional<boost::asio::ip::address_v6> ("ipc_address", ipc_address_l, boost::asio::ip::address_v6::loopback ());
			rpc_process.ipc_address = address_l.to_string ();
			rpc_process_l->get_optional<unsigned> ("num_ipc_connections", rpc_process.num_ipc_connections);
		}
	}

	return toml.get_error ();
}

nano::rpc_process_config::rpc_process_config (nano::network_constants & network_constants) :
	network_constants{ network_constants },
	ipc_address{ boost::asio::ip::address_v6::loopback ().to_string () }
{
}

namespace nano
{
nano::error read_rpc_config_toml (boost::filesystem::path const & data_path_a, nano::rpc_config & config_a, std::vector<std::string> const & config_overrides)
{
	nano::error error;
	auto toml_config_path = nano::get_rpc_toml_config_path (data_path_a);

	// Parse and deserialize
	nano::tomlconfig toml;

	std::stringstream config_overrides_stream;
	for (auto const & entry : config_overrides)
	{
		config_overrides_stream << entry << std::endl;
	}
	config_overrides_stream << std::endl;

	// Make sure we don't create an empty toml file if it doesn't exist. Running without a toml file is the default.
	if (!error)
	{
		if (boost::filesystem::exists (toml_config_path))
		{
			error = toml.read (config_overrides_stream, toml_config_path);
		}
		else
		{
			error = toml.read (config_overrides_stream);
		}
	}

	if (!error)
	{
		error = config_a.deserialize_toml (toml);
	}

	return error;
}
}
