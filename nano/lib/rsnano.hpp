#ifndef rs_nano_bindings_hpp
#define rs_nano_bindings_hpp

/* Warning, this file is autogenerated by cbindgen. Don't modify this manually. */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <ostream>

namespace rsnano
{
struct BandwidthLimiterHandle;

struct ChangeBlockHandle;

struct OpenBlockHandle;

struct ReceiveBlockHandle;

struct SendBlockHandle;

struct StateBlockHandle;

struct BlockDetailsDto
{
	uint8_t epoch;
	bool is_send;
	bool is_receive;
	bool is_epoch;
};

struct BlockSidebandDto
{
	uint64_t height;
	uint64_t timestamp;
	uint8_t successor[32];
	uint8_t account[32];
	uint8_t balance[16];
	BlockDetailsDto details;
	uint8_t source_epoch;
};

using Blake2BFinalCallback = int32_t (*) (void *, void *, uintptr_t);

using Blake2BInitCallback = int32_t (*) (void *, uintptr_t);

using Blake2BUpdateCallback = int32_t (*) (void *, const void *, uintptr_t);

using ReadBytesCallback = int32_t (*) (void *, uint8_t *, uintptr_t);

using ReadU8Callback = int32_t (*) (void *, uint8_t *);

using WriteBytesCallback = int32_t (*) (void *, const uint8_t *, uintptr_t);

using WriteU8Callback = int32_t (*) (void *, uint8_t);

struct ChangeBlockDto
{
	uint64_t work;
	uint8_t signature[64];
	uint8_t previous[32];
	uint8_t representative[32];
};

struct OpenBlockDto
{
	uint64_t work;
	uint8_t signature[64];
	uint8_t source[32];
	uint8_t representative[32];
	uint8_t account[32];
};

struct ReceiveBlockDto
{
	uint64_t work;
	uint8_t signature[64];
	uint8_t previous[32];
	uint8_t source[32];
};

struct SendBlockDto
{
	uint8_t previous[32];
	uint8_t destination[32];
	uint8_t balance[16];
	uint8_t signature[64];
	uint64_t work;
};

struct StateBlockDto
{
	uint8_t signature[64];
	uint8_t account[32];
	uint8_t previous[32];
	uint8_t representative[32];
	uint8_t link[32];
	uint8_t balance[16];
	uint64_t work;
};

extern "C" {

BandwidthLimiterHandle * rsn_bandwidth_limiter_create (double limit_burst_ratio, uintptr_t limit);

void rsn_bandwidth_limiter_destroy (BandwidthLimiterHandle * limiter);

int32_t rsn_bandwidth_limiter_reset (const BandwidthLimiterHandle * limiter,
double limit_burst_ratio,
uintptr_t limit);

bool rsn_bandwidth_limiter_should_drop (const BandwidthLimiterHandle * limiter,
uintptr_t message_size,
int32_t * result);

int32_t rsn_block_details_create (uint8_t epoch,
bool is_send,
bool is_receive,
bool is_epoch,
BlockDetailsDto * result);

int32_t rsn_block_details_deserialize (BlockDetailsDto * dto, void * stream);

int32_t rsn_block_details_serialize (const BlockDetailsDto * dto, void * stream);

int32_t rsn_block_sideband_deserialize (BlockSidebandDto * dto, void * stream, uint8_t block_type);

int32_t rsn_block_sideband_serialize (const BlockSidebandDto * dto, void * stream, uint8_t block_type);

uintptr_t rsn_block_sideband_size (uint8_t block_type, int32_t * result);

void rsn_callback_blake2b_final (Blake2BFinalCallback f);

void rsn_callback_blake2b_init (Blake2BInitCallback f);

void rsn_callback_blake2b_update (Blake2BUpdateCallback f);

void rsn_callback_read_bytes (ReadBytesCallback f);

void rsn_callback_read_u8 (ReadU8Callback f);

void rsn_callback_write_bytes (WriteBytesCallback f);

void rsn_callback_write_u8 (WriteU8Callback f);

ChangeBlockHandle * rsn_change_block_clone (const ChangeBlockHandle * handle);

ChangeBlockHandle * rsn_change_block_create (const ChangeBlockDto * dto);

int32_t rsn_change_block_deserialize (ChangeBlockHandle * handle, void * stream);

void rsn_change_block_destroy (ChangeBlockHandle * handle);

bool rsn_change_block_equals (const ChangeBlockHandle * a, const ChangeBlockHandle * b);

int32_t rsn_change_block_hash (const ChangeBlockHandle * handle, void * state);

void rsn_change_block_previous (const ChangeBlockHandle * handle, uint8_t (*result)[32]);

void rsn_change_block_previous_set (ChangeBlockHandle * handle, const uint8_t (*source)[32]);

void rsn_change_block_representative (const ChangeBlockHandle * handle, uint8_t (*result)[32]);

void rsn_change_block_representative_set (ChangeBlockHandle * handle,
const uint8_t (*representative)[32]);

int32_t rsn_change_block_serialize (ChangeBlockHandle * handle, void * stream);

void rsn_change_block_signature (const ChangeBlockHandle * handle, uint8_t (*result)[64]);

void rsn_change_block_signature_set (ChangeBlockHandle * handle, const uint8_t (*signature)[64]);

uintptr_t rsn_change_block_size ();

uint64_t rsn_change_block_work (const ChangeBlockHandle * handle);

void rsn_change_block_work_set (ChangeBlockHandle * handle, uint64_t work);

void rsn_open_block_account (const OpenBlockHandle * handle, uint8_t (*result)[32]);

void rsn_open_block_account_set (OpenBlockHandle * handle, const uint8_t (*account)[32]);

OpenBlockHandle * rsn_open_block_clone (const OpenBlockHandle * handle);

OpenBlockHandle * rsn_open_block_create (const OpenBlockDto * dto);

int32_t rsn_open_block_deserialize (OpenBlockHandle * handle, void * stream);

void rsn_open_block_destroy (OpenBlockHandle * handle);

bool rsn_open_block_equals (const OpenBlockHandle * a, const OpenBlockHandle * b);

int32_t rsn_open_block_hash (const OpenBlockHandle * handle, void * state);

void rsn_open_block_representative (const OpenBlockHandle * handle, uint8_t (*result)[32]);

void rsn_open_block_representative_set (OpenBlockHandle * handle,
const uint8_t (*representative)[32]);

int32_t rsn_open_block_serialize (OpenBlockHandle * handle, void * stream);

void rsn_open_block_signature (const OpenBlockHandle * handle, uint8_t (*result)[64]);

void rsn_open_block_signature_set (OpenBlockHandle * handle, const uint8_t (*signature)[64]);

uintptr_t rsn_open_block_size ();

void rsn_open_block_source (const OpenBlockHandle * handle, uint8_t (*result)[32]);

void rsn_open_block_source_set (OpenBlockHandle * handle, const uint8_t (*source)[32]);

uint64_t rsn_open_block_work (const OpenBlockHandle * handle);

void rsn_open_block_work_set (OpenBlockHandle * handle, uint64_t work);

ReceiveBlockHandle * rsn_receive_block_clone (const ReceiveBlockHandle * handle);

ReceiveBlockHandle * rsn_receive_block_create (const ReceiveBlockDto * dto);

void rsn_receive_block_destroy (ReceiveBlockHandle * handle);

bool rsn_receive_block_equals (const ReceiveBlockHandle * a, const ReceiveBlockHandle * b);

int32_t rsn_receive_block_hash (const ReceiveBlockHandle * handle, void * state);

void rsn_receive_block_previous (const ReceiveBlockHandle * handle, uint8_t (*result)[32]);

void rsn_receive_block_previous_set (ReceiveBlockHandle * handle, const uint8_t (*previous)[32]);

void rsn_receive_block_signature (const ReceiveBlockHandle * handle, uint8_t (*result)[64]);

void rsn_receive_block_signature_set (ReceiveBlockHandle * handle, const uint8_t (*signature)[64]);

uintptr_t rsn_receive_block_size ();

void rsn_receive_block_source (const ReceiveBlockHandle * handle, uint8_t (*result)[32]);

void rsn_receive_block_source_set (ReceiveBlockHandle * handle, const uint8_t (*previous)[32]);

uint64_t rsn_receive_block_work (const ReceiveBlockHandle * handle);

void rsn_receive_block_work_set (ReceiveBlockHandle * handle, uint64_t work);

void rsn_send_block_balance (const SendBlockHandle * handle, uint8_t (*result)[16]);

void rsn_send_block_balance_set (SendBlockHandle * handle, const uint8_t (*balance)[16]);

SendBlockHandle * rsn_send_block_clone (const SendBlockHandle * handle);

SendBlockHandle * rsn_send_block_create (const SendBlockDto * dto);

int32_t rsn_send_block_deserialize (SendBlockHandle * handle, void * stream);

void rsn_send_block_destination (const SendBlockHandle * handle, uint8_t (*result)[32]);

void rsn_send_block_destination_set (SendBlockHandle * handle, const uint8_t (*destination)[32]);

void rsn_send_block_destroy (SendBlockHandle * handle);

bool rsn_send_block_equals (const SendBlockHandle * a, const SendBlockHandle * b);

int32_t rsn_send_block_hash (const SendBlockHandle * handle, void * state);

void rsn_send_block_previous (const SendBlockHandle * handle, uint8_t (*result)[32]);

void rsn_send_block_previous_set (SendBlockHandle * handle, const uint8_t (*previous)[32]);

int32_t rsn_send_block_serialize (SendBlockHandle * handle, void * stream);

void rsn_send_block_signature (const SendBlockHandle * handle, uint8_t (*result)[64]);

void rsn_send_block_signature_set (SendBlockHandle * handle, const uint8_t (*signature)[64]);

uintptr_t rsn_send_block_size ();

bool rsn_send_block_valid_predecessor (uint8_t block_type);

uint64_t rsn_send_block_work (const SendBlockHandle * handle);

void rsn_send_block_work_set (SendBlockHandle * handle, uint64_t work);

void rsn_send_block_zero (SendBlockHandle * handle);

void rsn_state_block_account (const StateBlockHandle * handle, uint8_t (*result)[32]);

void rsn_state_block_account_set (StateBlockHandle * handle, const uint8_t (*source)[32]);

void rsn_state_block_balance (const StateBlockHandle * handle, uint8_t (*result)[16]);

void rsn_state_block_balance_set (StateBlockHandle * handle, const uint8_t (*representative)[16]);

StateBlockHandle * rsn_state_block_clone (const StateBlockHandle * handle);

StateBlockHandle * rsn_state_block_create (const StateBlockDto * dto);

int32_t rsn_state_block_deserialize (StateBlockHandle * handle, void * stream);

void rsn_state_block_destroy (StateBlockHandle * handle);

bool rsn_state_block_equals (const StateBlockHandle * a, const StateBlockHandle * b);

int32_t rsn_state_block_hash (const StateBlockHandle * handle, void * state);

void rsn_state_block_link (const StateBlockHandle * handle, uint8_t (*result)[32]);

void rsn_state_block_link_set (StateBlockHandle * handle, const uint8_t (*representative)[32]);

void rsn_state_block_previous (const StateBlockHandle * handle, uint8_t (*result)[32]);

void rsn_state_block_previous_set (StateBlockHandle * handle, const uint8_t (*source)[32]);

void rsn_state_block_representative (const StateBlockHandle * handle, uint8_t (*result)[32]);

void rsn_state_block_representative_set (StateBlockHandle * handle,
const uint8_t (*representative)[32]);

int32_t rsn_state_block_serialize (StateBlockHandle * handle, void * stream);

void rsn_state_block_signature (const StateBlockHandle * handle, uint8_t (*result)[64]);

void rsn_state_block_signature_set (StateBlockHandle * handle, const uint8_t (*signature)[64]);

uintptr_t rsn_state_block_size ();

uint64_t rsn_state_block_work (const StateBlockHandle * handle);

void rsn_state_block_work_set (StateBlockHandle * handle, uint64_t work);

} // extern "C"

} // namespace rsnano

#endif // rs_nano_bindings_hpp
