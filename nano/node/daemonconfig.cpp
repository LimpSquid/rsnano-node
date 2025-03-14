#include <nano/lib/config.hpp>
#include <nano/lib/jsonconfig.hpp>
#include <nano/lib/tomlconfig.hpp>
#include <nano/lib/walletconfig.hpp>
#include <nano/node/daemonconfig.hpp>

#include <sstream>
#include <vector>

nano::daemon_config::daemon_config (boost::filesystem::path const & data_path_a, nano::network_params & network_params) :
	node{ network_params },
	data_path{ data_path_a }
{
	rsnano::DaemonConfigDto dto;
	auto network_dto{ network_params.to_dto () };
	if (rsnano::rsn_daemon_config_create (&dto, &network_dto.dto) < 0)
		throw std::runtime_error ("could not create daemon_config");
	rpc_enable = dto.rpc_enable;
	node.load_dto (dto.node);
	opencl.load_dto (dto.opencl);
	opencl_enable = dto.opencl_enable;
	pow_server.load_dto (dto.pow_server);
	rpc.load_dto (dto.rpc);
}

rsnano::DaemonConfigDto to_daemon_config_dto (nano::daemon_config const & config)
{
	rsnano::DaemonConfigDto dto;
	dto.rpc_enable = config.rpc_enable;
	dto.opencl = config.opencl.to_dto ();
	dto.node = config.node.to_dto ();
	dto.opencl_enable = config.opencl_enable;
	dto.pow_server = config.pow_server.to_dto ();
	dto.rpc = config.rpc.to_dto ();
	return dto;
}

nano::error nano::daemon_config::serialize_toml (nano::tomlconfig & toml)
{
	auto dto{ to_daemon_config_dto (*this) };
	if (rsnano::rsn_daemon_config_serialize_toml (&dto, &toml) < 0)
		return nano::error ("could not TOML serialize daemon_config");

	return toml.get_error ();
}

nano::error nano::daemon_config::deserialize_toml (nano::tomlconfig & toml)
{
	auto rpc_l (toml.get_optional_child ("rpc"));
	if (!toml.get_error () && rpc_l)
	{
		rpc_l->get_optional<bool> ("enable", rpc_enable);
		rpc.deserialize_toml (*rpc_l);
	}

	auto node_l (toml.get_optional_child ("node"));
	if (!toml.get_error () && node_l)
	{
		node.deserialize_toml (*node_l);
	}

	auto opencl_l (toml.get_optional_child ("opencl"));
	if (!toml.get_error () && opencl_l)
	{
		opencl_l->get_optional<bool> ("enable", opencl_enable);
		opencl.deserialize_toml (*opencl_l);
	}

	auto pow_l (toml.get_optional_child ("nano_pow_server"));
	if (!toml.get_error () && pow_l)
	{
		pow_server.deserialize_toml (*pow_l);
	}

	return toml.get_error ();
}

nano::error nano::read_node_config_toml (boost::filesystem::path const & data_path_a, nano::daemon_config & config_a, std::vector<std::string> const & config_overrides)
{
	nano::error error;
	auto toml_config_path = nano::get_node_toml_config_path (data_path_a);
	auto toml_qt_config_path = nano::get_qtwallet_toml_config_path (data_path_a);

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
