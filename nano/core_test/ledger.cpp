#include <nano/lib/stats.hpp>
#include <nano/lib/threading.hpp>
#include <nano/node/election.hpp>
#include <nano/node/transport/inproc.hpp>
#include <nano/test_common/ledger.hpp>
#include <nano/test_common/system.hpp>
#include <nano/test_common/testutil.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST (ledger, process_modifies_sideband)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*store.tx_begin_write (), *send1).code);
	ASSERT_EQ (send1->sideband ().timestamp (), store.block ().get (*store.tx_begin_read (), send1->hash ())->sideband ().timestamp ());
}

// Create a send block and publish it.
TEST (ledger, process_send)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::keypair key2;
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (info1.head ())
				.destination (key2.pub)
				.balance (50)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (info1.head ()))
				.build ();
	nano::block_hash hash1 = send->hash ();
	ASSERT_EQ (nano::dev::genesis_key.pub, store.frontier ().get (*transaction, info1.head ()));
	ASSERT_EQ (1, info1.block_count ());
	// This was a valid block, it should progress.
	auto return1 = ledger.process (*transaction, *send);
	ASSERT_EQ (nano::dev::genesis_key.pub, send->sideband ().account ());
	ASSERT_EQ (2, send->sideband ().height ());
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.amount (*transaction, hash1));
	ASSERT_TRUE (store.frontier ().get (*transaction, info1.head ()).is_zero ());
	ASSERT_EQ (nano::dev::genesis_key.pub, store.frontier ().get (*transaction, hash1));
	ASSERT_EQ (nano::process_result::progress, return1.code);
	ASSERT_EQ (nano::dev::genesis_key.pub, store.block ().account_calculated (*send));
	ASSERT_EQ (50, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.account_receivable (*transaction, key2.pub));
	nano::account_info info2;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info2));
	ASSERT_EQ (2, info2.block_count ());
	auto latest6 = store.block ().get (*transaction, info2.head ());
	ASSERT_NE (nullptr, latest6);
	auto latest7 = dynamic_cast<nano::send_block *> (latest6.get ());
	ASSERT_NE (nullptr, latest7);
	ASSERT_EQ (*send, *latest7);
	// Create an open block opening an account accepting the send we just created
	auto open = builder
				.open ()
				.source (hash1)
				.representative (key2.pub)
				.account (key2.pub)
				.sign (key2.prv, key2.pub)
				.work (*pool.generate (key2.pub))
				.build ();
	nano::block_hash hash2 (open->hash ());
	// This was a valid block, it should progress.
	auto return2 = ledger.process (*transaction, *open);
	ASSERT_EQ (nano::process_result::progress, return2.code);
	ASSERT_EQ (key2.pub, open->sideband ().account ());
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, open->sideband ().balance ().number ());
	ASSERT_EQ (1, open->sideband ().height ());
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.amount (*transaction, hash2));
	ASSERT_EQ (nano::process_result::progress, return2.code);
	ASSERT_EQ (key2.pub, store.block ().account_calculated (*open));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.amount (*transaction, hash2));
	ASSERT_EQ (key2.pub, store.frontier ().get (*transaction, hash2));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.account_balance (*transaction, key2.pub));
	ASSERT_EQ (0, ledger.account_receivable (*transaction, key2.pub));
	ASSERT_EQ (50, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.weight (key2.pub));
	nano::account_info info3;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info3));
	auto latest2 = store.block ().get (*transaction, info3.head ());
	ASSERT_NE (nullptr, latest2);
	auto latest3 = dynamic_cast<nano::send_block *> (latest2.get ());
	ASSERT_NE (nullptr, latest3);
	ASSERT_EQ (*send, *latest3);
	nano::account_info info4;
	ASSERT_FALSE (store.account ().get (*transaction, key2.pub, info4));
	auto latest4 = store.block ().get (*transaction, info4.head ());
	ASSERT_NE (nullptr, latest4);
	auto latest5 = dynamic_cast<nano::open_block *> (latest4.get ());
	ASSERT_NE (nullptr, latest5);
	ASSERT_EQ (*open, *latest5);
	ASSERT_FALSE (ledger.rollback (*transaction, hash2));
	ASSERT_TRUE (store.frontier ().get (*transaction, hash2).is_zero ());
	nano::account_info info5;
	ASSERT_TRUE (ledger.store.account ().get (*transaction, key2.pub, info5));
	nano::pending_info pending1;
	ASSERT_FALSE (ledger.store.pending ().get (*transaction, nano::pending_key (key2.pub, hash1), pending1));
	ASSERT_EQ (nano::dev::genesis_key.pub, pending1.source);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, pending1.amount.number ());
	ASSERT_EQ (0, ledger.account_balance (*transaction, key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.account_receivable (*transaction, key2.pub));
	ASSERT_EQ (50, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (50, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	nano::account_info info6;
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis_key.pub, info6));
	ASSERT_EQ (hash1, info6.head ());
	ASSERT_FALSE (ledger.rollback (*transaction, info6.head ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::genesis_key.pub, store.frontier ().get (*transaction, info1.head ()));
	ASSERT_TRUE (store.frontier ().get (*transaction, hash1).is_zero ());
	nano::account_info info7;
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis_key.pub, info7));
	ASSERT_EQ (1, info7.block_count ());
	ASSERT_EQ (info1.head (), info7.head ());
	nano::pending_info pending2;
	ASSERT_TRUE (ledger.store.pending ().get (*transaction, nano::pending_key (key2.pub, hash1), pending2));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.account_receivable (*transaction, key2.pub));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, process_receive)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::keypair key2;
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (info1.head ())
				.destination (key2.pub)
				.balance (50)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (info1.head ()))
				.build ();
	nano::block_hash hash1 (send->hash ());
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	nano::keypair key3;
	auto open = builder
				.open ()
				.source (hash1)
				.representative (key3.pub)
				.account (key2.pub)
				.sign (key2.prv, key2.pub)
				.work (*pool.generate (key2.pub))
				.build ();
	nano::block_hash hash2 (open->hash ());
	auto return1 = ledger.process (*transaction, *open);
	ASSERT_EQ (nano::process_result::progress, return1.code);
	ASSERT_EQ (key2.pub, store.block ().account_calculated (*open));
	ASSERT_EQ (key2.pub, open->sideband ().account ());
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, open->sideband ().balance ().number ());
	ASSERT_EQ (1, open->sideband ().height ());
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.amount (*transaction, hash2));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.weight (key3.pub));
	auto send2 = builder
				 .send ()
				 .previous (hash1)
				 .destination (key2.pub)
				 .balance (25)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (hash1))
				 .build ();
	nano::block_hash hash3 = send2->hash ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	auto receive = builder
				   .receive ()
				   .previous (hash2)
				   .source (hash3)
				   .sign (key2.prv, key2.pub)
				   .work (*pool.generate (hash2))
				   .build ();
	auto hash4 = receive->hash ();
	ASSERT_EQ (key2.pub, store.frontier ().get (*transaction, hash2));
	auto return2 = ledger.process (*transaction, *receive);
	ASSERT_EQ (key2.pub, receive->sideband ().account ());
	ASSERT_EQ (nano::dev::constants.genesis_amount - 25, receive->sideband ().balance ().number ());
	ASSERT_EQ (2, receive->sideband ().height ());
	ASSERT_EQ (25, ledger.amount (*transaction, hash4));
	ASSERT_TRUE (store.frontier ().get (*transaction, hash2).is_zero ());
	ASSERT_EQ (key2.pub, store.frontier ().get (*transaction, hash4));
	ASSERT_EQ (nano::process_result::progress, return2.code);
	ASSERT_EQ (key2.pub, store.block ().account_calculated (*receive));
	ASSERT_EQ (hash4, ledger.latest (*transaction, key2.pub));
	ASSERT_EQ (25, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.account_receivable (*transaction, key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 25, ledger.account_balance (*transaction, key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 25, ledger.weight (key3.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, hash4));
	ASSERT_TRUE (store.block ().successor (*transaction, hash2).is_zero ());
	ASSERT_EQ (key2.pub, store.frontier ().get (*transaction, hash2));
	ASSERT_TRUE (store.frontier ().get (*transaction, hash4).is_zero ());
	ASSERT_EQ (25, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (25, ledger.account_receivable (*transaction, key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.account_balance (*transaction, key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.weight (key3.pub));
	ASSERT_EQ (hash2, ledger.latest (*transaction, key2.pub));
	nano::pending_info pending1;
	ASSERT_FALSE (ledger.store.pending ().get (*transaction, nano::pending_key (key2.pub, hash3), pending1));
	ASSERT_EQ (nano::dev::genesis_key.pub, pending1.source);
	ASSERT_EQ (25, pending1.amount.number ());
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, rollback_receiver)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::keypair key2;
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (info1.head ())
				.destination (key2.pub)
				.balance (50)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (info1.head ()))
				.build ();
	nano::block_hash hash1 (send->hash ());
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	nano::keypair key3;
	auto open = builder
				.open ()
				.source (hash1)
				.representative (key3.pub)
				.account (key2.pub)
				.sign (key2.prv, key2.pub)
				.work (*pool.generate (key2.pub))
				.build ();
	nano::block_hash hash2 (open->hash ());
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open).code);
	ASSERT_EQ (hash2, ledger.latest (*transaction, key2.pub));
	ASSERT_EQ (50, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.account_balance (*transaction, key2.pub));
	ASSERT_EQ (50, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.weight (key3.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, hash1));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.account_balance (*transaction, nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.account_balance (*transaction, key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (key3.pub));
	nano::account_info info2;
	ASSERT_TRUE (store.account ().get (*transaction, key2.pub, info2));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
	nano::pending_info pending1;
	ASSERT_TRUE (store.pending ().get (*transaction, nano::pending_key{ key2.pub, info2.head () }, pending1));
}

TEST (ledger, rollback_representation)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key5;
	nano::block_builder builder;
	auto change1 = builder
				   .change ()
				   .previous (nano::dev::genesis->hash ())
				   .representative (key5.pub)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (nano::dev::genesis->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change1).code);
	nano::keypair key3;
	auto change2 = builder
				   .change ()
				   .previous (change1->hash ())
				   .representative (key3.pub)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (change1->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change2).code);
	nano::keypair key2;
	auto send1 = builder
				 .send ()
				 .previous (change2->hash ())
				 .destination (key2.pub)
				 .balance (50)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (change2->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	nano::keypair key4;
	auto open = builder
				.open ()
				.source (send1->hash ())
				.representative (key4.pub)
				.account (key2.pub)
				.sign (key2.prv, key2.pub)
				.work (*pool.generate (key2.pub))
				.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open).code);
	auto send2 = builder
				 .send ()
				 .previous (send1->hash ())
				 .destination (key2.pub)
				 .balance (1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	auto receive1 = builder
					.receive ()
					.previous (open->hash ())
					.source (send2->hash ())
					.sign (key2.prv, key2.pub)
					.work (*pool.generate (open->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_EQ (1, ledger.weight (key3.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 1, ledger.weight (key4.pub));
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, key2.pub, info1));
	ASSERT_EQ (key4.pub, info1.representative ());
	ASSERT_FALSE (ledger.rollback (*transaction, receive1->hash ()));
	nano::account_info info2;
	ASSERT_FALSE (store.account ().get (*transaction, key2.pub, info2));
	ASSERT_EQ (key4.pub, info2.representative ());
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.weight (key4.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, open->hash ()));
	ASSERT_EQ (1, ledger.weight (key3.pub));
	ASSERT_EQ (0, ledger.weight (key4.pub));
	ledger.rollback (*transaction, send1->hash ());
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (key3.pub));
	nano::account_info info3;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info3));
	ASSERT_EQ (key3.pub, info3.representative ());
	ASSERT_FALSE (ledger.rollback (*transaction, change2->hash ()));
	nano::account_info info4;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info4));
	ASSERT_EQ (key5.pub, info4.representative ());
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (key5.pub));
	ASSERT_EQ (0, ledger.weight (key3.pub));
}

TEST (ledger, receive_rollback)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (nano::dev::genesis->hash ())
				.destination (nano::dev::genesis_key.pub)
				.balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (nano::dev::genesis->hash ()))
				.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	auto receive = builder
				   .receive ()
				   .previous (send->hash ())
				   .source (send->hash ())
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (send->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive).code);
	ASSERT_FALSE (ledger.rollback (*transaction, receive->hash ()));
}

TEST (ledger, process_duplicate)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::keypair key2;
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (info1.head ())
				.destination (key2.pub)
				.balance (50)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (info1.head ()))
				.build ();
	nano::block_hash hash1 (send->hash ());
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	ASSERT_EQ (nano::process_result::old, ledger.process (*transaction, *send).code);
	auto open = builder
				.open ()
				.source (hash1)
				.representative (1)
				.account (key2.pub)
				.sign (key2.prv, key2.pub)
				.work (*pool.generate (key2.pub))
				.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open).code);
	ASSERT_EQ (nano::process_result::old, ledger.process (*transaction, *open).code);
}

TEST (ledger, representative_genesis)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	auto latest = ledger.latest (*transaction, nano::dev::genesis_key.pub);
	ASSERT_FALSE (latest.is_zero ());
	ASSERT_EQ (nano::dev::genesis->hash (), ledger.representative (*transaction, latest));
}

TEST (ledger, weight)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
}

TEST (ledger, representative_change)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::keypair key2;
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::block_builder builder;
	auto block = builder
				 .change ()
				 .previous (info1.head ())
				 .representative (key2.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (info1.head ()))
				 .build ();
	ASSERT_EQ (nano::dev::genesis_key.pub, store.frontier ().get (*transaction, info1.head ()));
	auto return1 (ledger.process (*transaction, *block));
	ASSERT_EQ (0, ledger.amount (*transaction, block->hash ()));
	ASSERT_TRUE (store.frontier ().get (*transaction, info1.head ()).is_zero ());
	ASSERT_EQ (nano::dev::genesis_key.pub, store.frontier ().get (*transaction, block->hash ()));
	ASSERT_EQ (nano::process_result::progress, return1.code);
	ASSERT_EQ (nano::dev::genesis_key.pub, store.block ().account_calculated (*block));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (key2.pub));
	nano::account_info info2;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info2));
	ASSERT_EQ (block->hash (), info2.head ());
	ASSERT_FALSE (ledger.rollback (*transaction, info2.head ()));
	ASSERT_EQ (nano::dev::genesis_key.pub, store.frontier ().get (*transaction, info1.head ()));
	ASSERT_TRUE (store.frontier ().get (*transaction, block->hash ()).is_zero ());
	nano::account_info info3;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info3));
	ASSERT_EQ (info1.head (), info3.head ());
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
}

TEST (ledger, send_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::keypair key2;
	nano::keypair key3;
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::block_builder builder;
	auto block = builder
				 .send ()
				 .previous (info1.head ())
				 .destination (key2.pub)
				 .balance (100)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (info1.head ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block).code);
	auto block2 = builder
				  .send ()
				  .previous (info1.head ())
				  .destination (key3.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (info1.head ()))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *block2).code);
}

TEST (ledger, receive_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::keypair key2;
	nano::keypair key3;
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::block_builder builder;
	auto block = builder
				 .send ()
				 .previous (info1.head ())
				 .destination (key2.pub)
				 .balance (100)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (info1.head ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block).code);
	auto block2 = builder
				  .open ()
				  .source (block->hash ())
				  .representative (key2.pub)
				  .account (key2.pub)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (key2.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	auto block3 = builder
				  .change ()
				  .previous (block2->hash ())
				  .representative (key3.pub)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (block2->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block3).code);
	auto block4 = builder
				  .send ()
				  .previous (block->hash ())
				  .destination (key2.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block4).code);
	auto block5 = builder
				  .receive ()
				  .previous (block2->hash ())
				  .source (block4->hash ())
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (block2->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *block5).code);
}

TEST (ledger, open_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::keypair key2;
	nano::keypair key3;
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::block_builder builder;
	auto block = builder
				 .send ()
				 .previous (info1.head ())
				 .destination (key2.pub)
				 .balance (100)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (info1.head ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block).code);
	auto block2 = builder
				  .open ()
				  .source (block->hash ())
				  .representative (key2.pub)
				  .account (key2.pub)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (key2.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	auto block3 = builder
				  .open ()
				  .source (block->hash ())
				  .representative (key3.pub)
				  .account (key2.pub)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (key2.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *block3).code);
}

TEST (ledger, representation)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto & rep_weights = ledger.cache.rep_weights ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	ASSERT_EQ (nano::dev::constants.genesis_amount, rep_weights.representation_get (nano::dev::genesis_key.pub));
	nano::keypair key2;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key2.pub)
				  .balance (nano::dev::constants.genesis_amount - 100)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, rep_weights.representation_get (nano::dev::genesis_key.pub));
	nano::keypair key3;
	auto block2 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (key3.pub)
				  .account (key2.pub)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (key2.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (100, rep_weights.representation_get (key3.pub));
	auto block3 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key2.pub)
				  .balance (nano::dev::constants.genesis_amount - 200)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block3).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (100, rep_weights.representation_get (key3.pub));
	auto block4 = builder
				  .receive ()
				  .previous (block2->hash ())
				  .source (block3->hash ())
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (block2->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block4).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (200, rep_weights.representation_get (key3.pub));
	nano::keypair key4;
	auto block5 = builder
				  .change ()
				  .previous (block4->hash ())
				  .representative (key4.pub)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (block4->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block5).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key3.pub));
	ASSERT_EQ (200, rep_weights.representation_get (key4.pub));
	nano::keypair key5;
	auto block6 = builder
				  .send ()
				  .previous (block5->hash ())
				  .destination (key5.pub)
				  .balance (100)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (block5->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block6).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key3.pub));
	ASSERT_EQ (100, rep_weights.representation_get (key4.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key5.pub));
	nano::keypair key6;
	auto block7 = builder
				  .open ()
				  .source (block6->hash ())
				  .representative (key6.pub)
				  .account (key5.pub)
				  .sign (key5.prv, key5.pub)
				  .work (*pool.generate (key5.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block7).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key3.pub));
	ASSERT_EQ (100, rep_weights.representation_get (key4.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key5.pub));
	ASSERT_EQ (100, rep_weights.representation_get (key6.pub));
	auto block8 = builder
				  .send ()
				  .previous (block6->hash ())
				  .destination (key5.pub)
				  .balance (0)
				  .sign (key2.prv, key2.pub)
				  .work (*pool.generate (block6->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block8).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key3.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key4.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key5.pub));
	ASSERT_EQ (100, rep_weights.representation_get (key6.pub));
	auto block9 = builder
				  .receive ()
				  .previous (block7->hash ())
				  .source (block8->hash ())
				  .sign (key5.prv, key5.pub)
				  .work (*pool.generate (block7->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block9).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 200, rep_weights.representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key2.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key3.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key4.pub));
	ASSERT_EQ (0, rep_weights.representation_get (key5.pub));
	ASSERT_EQ (200, rep_weights.representation_get (key6.pub));
}

TEST (ledger, double_open)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key2;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key2.pub)
				 .balance (1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (key2.pub)
				 .account (key2.pub)
				 .sign (key2.prv, key2.pub)
				 .work (*pool.generate (key2.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	auto open2 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis_key.pub)
				 .account (key2.pub)
				 .sign (key2.prv, key2.pub)
				 .work (*pool.generate (key2.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *open2).code);
}

TEST (ledger, double_receive)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key2;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key2.pub)
				 .balance (1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (key2.pub)
				 .account (key2.pub)
				 .sign (key2.prv, key2.pub)
				 .work (*pool.generate (key2.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	auto receive1 = builder
					.receive ()
					.previous (open1->hash ())
					.source (send1->hash ())
					.sign (key2.prv, key2.pub)
					.work (*pool.generate (open1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *receive1).code);
}

TEST (votes, check_signature)
{
	nano::test::system system;
	nano::node_config node_config (nano::test::get_available_port (), system.logging);
	node_config.online_weight_minimum = std::numeric_limits<nano::uint128_t>::max ();
	auto & node1 = *system.add_node (node_config);
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	{
		auto transaction (node1.store.tx_begin_write ());
		ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send1).code);
	}
	node1.scheduler.activate (nano::dev::genesis_key.pub, *node1.store.tx_begin_read ());
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()));
	auto election1 = node1.active.election (send1->qualified_root ());
	ASSERT_EQ (1, election1->votes ().size ());
	auto vote1 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 1, 0, std::vector<nano::block_hash>{ send1->hash () }));
	vote1->flip_signature_bit_0 ();
	ASSERT_EQ (nano::vote_code::invalid, node1.vote_processor.vote_blocking (vote1, std::make_shared<nano::transport::inproc::channel> (node1, node1)));
	vote1->flip_signature_bit_0 ();
	ASSERT_EQ (nano::vote_code::vote, node1.vote_processor.vote_blocking (vote1, std::make_shared<nano::transport::inproc::channel> (node1, node1)));
	ASSERT_EQ (nano::vote_code::replay, node1.vote_processor.vote_blocking (vote1, std::make_shared<nano::transport::inproc::channel> (node1, node1)));
}

TEST (votes, add_one)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto transaction (node1.store.tx_begin_write ());
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send1).code);
	node1.block_confirm (send1);
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()));
	auto election1 = node1.active.election (send1->qualified_root ());
	ASSERT_EQ (1, election1->votes ().size ());
	auto vote1 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 1, 0, std::vector<nano::block_hash>{ send1->hash () }));
	ASSERT_EQ (nano::vote_code::vote, node1.active.vote (vote1));
	auto vote2 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 2, 0, std::vector<nano::block_hash>{ send1->hash () }));
	ASSERT_EQ (nano::vote_code::vote, node1.active.vote (vote2));
	ASSERT_EQ (2, election1->votes ().size ());
	auto votes1 (election1->votes ());
	auto existing1 (votes1.find (nano::dev::genesis_key.pub));
	ASSERT_NE (votes1.end (), existing1);
	ASSERT_EQ (send1->hash (), existing1->second.hash);
	nano::lock_guard<nano::mutex> guard (node1.active.mutex);
	auto winner (*election1->tally ().begin ());
	ASSERT_EQ (*send1, *winner.second);
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, winner.first);
}

namespace nano
{
// Higher timestamps change the vote
TEST (votes, add_existing)
{
	nano::test::system system;
	nano::node_config node_config (nano::test::get_available_port (), system.logging);
	node_config.online_weight_minimum = nano::dev::constants.genesis_amount;
	node_config.frontiers_confirmation = nano::frontiers_confirmation_mode::disabled;
	auto & node1 = *system.add_node (node_config);
	nano::keypair key1;
	nano::block_builder builder;
	std::shared_ptr<nano::block> send1 = builder.state ()
										 .account (nano::dev::genesis_key.pub)
										 .previous (nano::dev::genesis->hash ())
										 .representative (nano::dev::genesis_key.pub) // No representative, blocks can't confirm
										 .balance (nano::dev::constants.genesis_amount / 2 - nano::Gxrb_ratio)
										 .link (key1.pub)
										 .work (0)
										 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
										 .build ();
	node1.work_generate_blocking (*send1);
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*node1.store.tx_begin_write (), *send1).code);
	node1.scheduler.activate (nano::dev::genesis_key.pub, *node1.store.tx_begin_read ());
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()));
	auto election1 = node1.active.election (send1->qualified_root ());
	auto vote1 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 1, 0, std::vector<nano::block_hash>{ send1->hash () }));
	ASSERT_EQ (nano::vote_code::vote, node1.active.vote (vote1));
	// Block is already processed from vote
	ASSERT_TRUE (node1.active.publish (send1));
	ASSERT_EQ (nano::vote::timestamp_min * 1, election1->last_votes[nano::dev::genesis_key.pub].timestamp);
	nano::keypair key2;
	std::shared_ptr<nano::block> send2 = builder.state ()
										 .account (nano::dev::genesis_key.pub)
										 .previous (nano::dev::genesis->hash ())
										 .representative (nano::dev::genesis_key.pub) // No representative, blocks can't confirm
										 .balance (nano::dev::constants.genesis_amount / 2 - nano::Gxrb_ratio)
										 .link (key2.pub)
										 .work (0)
										 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
										 .build ();
	node1.work_generate_blocking (*send2);
	ASSERT_FALSE (node1.active.publish (send2));
	ASSERT_TIMELY (5s, node1.active.active (*send2));
	auto vote2 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 2, 0, std::vector<nano::block_hash>{ send2->hash () }));
	// Pretend we've waited the timeout
	nano::unique_lock<nano::mutex> lock (election1->mutex);
	election1->last_votes[nano::dev::genesis_key.pub].time = std::chrono::steady_clock::now () - std::chrono::seconds (20);
	lock.unlock ();
	ASSERT_EQ (nano::vote_code::vote, node1.active.vote (vote2));
	ASSERT_EQ (nano::vote::timestamp_min * 2, election1->last_votes[nano::dev::genesis_key.pub].timestamp);
	// Also resend the old vote, and see if we respect the timestamp
	lock.lock ();
	election1->last_votes[nano::dev::genesis_key.pub].time = std::chrono::steady_clock::now () - std::chrono::seconds (20);
	lock.unlock ();
	ASSERT_EQ (nano::vote_code::replay, node1.active.vote (vote1));
	ASSERT_EQ (nano::vote::timestamp_min * 2, election1->votes ()[nano::dev::genesis_key.pub].timestamp);
	auto votes (election1->votes ());
	ASSERT_EQ (2, votes.size ());
	ASSERT_NE (votes.end (), votes.find (nano::dev::genesis_key.pub));
	ASSERT_EQ (send2->hash (), votes[nano::dev::genesis_key.pub].hash);
	ASSERT_EQ (*send2, *election1->tally ().begin ()->second);
}

// Lower timestamps are ignored
TEST (votes, add_old)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto transaction (node1.store.tx_begin_write ());
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send1).code);
	node1.block_confirm (send1);
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()));
	auto election1 = node1.active.election (send1->qualified_root ());
	auto vote1 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 2, 0, std::vector<nano::block_hash>{ send1->hash () }));
	auto channel (std::make_shared<nano::transport::inproc::channel> (node1, node1));
	node1.vote_processor.vote_blocking (vote1, channel);
	nano::keypair key2;
	auto send2 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key2.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send2);
	auto vote2 = std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 1, 0, std::vector<nano::block_hash>{ send2->hash () });
	{
		nano::lock_guard<nano::mutex> lock (election1->mutex);
		election1->last_votes[nano::dev::genesis_key.pub].time = std::chrono::steady_clock::now () - std::chrono::seconds (20);
	}
	node1.vote_processor.vote_blocking (vote2, channel);
	ASSERT_EQ (2, election1->votes ().size ());
	auto votes (election1->votes ());
	ASSERT_NE (votes.end (), votes.find (nano::dev::genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes[nano::dev::genesis_key.pub].hash);
	ASSERT_EQ (*send1, *election1->winner ());
}
}

// Lower timestamps are accepted for different accounts
// Test disabled because it's failing intermittently.
// PR in which it got disabled: https://github.com/nanocurrency/nano-node/pull/3629
// Issue for investigating it: https://github.com/nanocurrency/nano-node/issues/3631
TEST (votes, DISABLED_add_old_different_account)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto send2 = builder
				 .send ()
				 .previous (send1->hash ())
				 .destination (key1.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send2);
	ASSERT_EQ (nano::process_result::progress, node1.process (*send1).code);
	ASSERT_EQ (nano::process_result::progress, node1.process (*send2).code);
	nano::test::blocks_confirm (node1, { send1, send2 });
	auto election1 = node1.active.election (send1->qualified_root ());
	ASSERT_NE (nullptr, election1);
	auto election2 = node1.active.election (send2->qualified_root ());
	ASSERT_NE (nullptr, election2);
	ASSERT_EQ (1, election1->votes ().size ());
	ASSERT_EQ (1, election2->votes ().size ());
	auto vote1 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 2, 0, std::vector<nano::block_hash>{ send1->hash () }));
	auto channel (std::make_shared<nano::transport::inproc::channel> (node1, node1));
	auto vote_result1 (node1.vote_processor.vote_blocking (vote1, channel));
	ASSERT_EQ (nano::vote_code::vote, vote_result1);
	ASSERT_EQ (2, election1->votes ().size ());
	ASSERT_EQ (1, election2->votes ().size ());
	auto vote2 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 1, 0, std::vector<nano::block_hash>{ send2->hash () }));
	auto vote_result2 (node1.vote_processor.vote_blocking (vote2, channel));
	ASSERT_EQ (nano::vote_code::vote, vote_result2);
	ASSERT_EQ (2, election1->votes ().size ());
	ASSERT_EQ (2, election2->votes ().size ());
	auto votes1 (election1->votes ());
	auto votes2 (election2->votes ());
	ASSERT_NE (votes1.end (), votes1.find (nano::dev::genesis_key.pub));
	ASSERT_NE (votes2.end (), votes2.find (nano::dev::genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes1[nano::dev::genesis_key.pub].hash);
	ASSERT_EQ (send2->hash (), votes2[nano::dev::genesis_key.pub].hash);
	ASSERT_EQ (*send1, *election1->winner ());
	ASSERT_EQ (*send2, *election2->winner ());
}

// The voting cooldown is respected
TEST (votes, add_cooldown)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto transaction (node1.store.tx_begin_write ());
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send1).code);
	node1.block_confirm (send1);
	ASSERT_TIMELY (5s, node1.active.election (send1->qualified_root ()));
	auto election1 = node1.active.election (send1->qualified_root ());
	auto vote1 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 1, 0, std::vector<nano::block_hash>{ send1->hash () }));
	auto channel (std::make_shared<nano::transport::inproc::channel> (node1, node1));
	node1.vote_processor.vote_blocking (vote1, channel);
	nano::keypair key2;
	auto send2 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key2.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send2);
	auto vote2 (std::make_shared<nano::vote> (nano::dev::genesis_key.pub, nano::dev::genesis_key.prv, nano::vote::timestamp_min * 2, 0, std::vector<nano::block_hash>{ send2->hash () }));
	node1.vote_processor.vote_blocking (vote2, channel);
	ASSERT_EQ (2, election1->votes ().size ());
	auto votes (election1->votes ());
	ASSERT_NE (votes.end (), votes.find (nano::dev::genesis_key.pub));
	ASSERT_EQ (send1->hash (), votes[nano::dev::genesis_key.pub].hash);
	ASSERT_EQ (*send1, *election1->winner ());
}

// Query for block successor
TEST (ledger, successor)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (0)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build ();
	node1.work_generate_blocking (*send1);
	auto transaction (node1.store.tx_begin_write ());
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send1).code);
	ASSERT_EQ (*send1, *node1.ledger.successor (*transaction, nano::qualified_root (nano::root (0), nano::dev::genesis->hash ())));
	ASSERT_EQ (*nano::dev::genesis, *node1.ledger.successor (*transaction, nano::dev::genesis->qualified_root ()));
	ASSERT_EQ (nullptr, node1.ledger.successor (*transaction, nano::qualified_root (0)));
}

TEST (ledger, fail_change_old)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block = builder
				 .change ()
				 .previous (nano::dev::genesis->hash ())
				 .representative (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	auto result1 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto result2 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::old, result2.code);
}

TEST (ledger, fail_change_gap_previous)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block = builder
				 .change ()
				 .previous (1)
				 .representative (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::root (1)))
				 .build ();
	auto result1 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::gap_previous, result1.code);
}

TEST (ledger, fail_change_bad_signature)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block = builder
				 .change ()
				 .previous (nano::dev::genesis->hash ())
				 .representative (key1.pub)
				 .sign (nano::keypair ().prv, 0)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	auto result1 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::bad_signature, result1.code);
}

TEST (ledger, fail_change_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .change ()
				  .previous (nano::dev::genesis->hash ())
				  .representative (key1.pub)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	nano::keypair key2;
	auto block2 = builder
				  .change ()
				  .previous (nano::dev::genesis->hash ())
				  .representative (key2.pub)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::fork, result2.code);
}

TEST (ledger, fail_send_old)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	auto result1 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto result2 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::old, result2.code);
}

TEST (ledger, fail_send_gap_previous)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block = builder
				 .send ()
				 .previous (1)
				 .destination (key1.pub)
				 .balance (1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::root (1)))
				 .build ();
	auto result1 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::gap_previous, result1.code);
}

TEST (ledger, fail_send_bad_signature)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (1)
				 .sign (nano::keypair ().prv, 0)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	auto result1 = ledger.process (*transaction, *block);
	ASSERT_EQ (nano::process_result::bad_signature, result1.code);
}

TEST (ledger, fail_send_negative_spend)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	nano::keypair key2;
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key2.pub)
				  .balance (2)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::negative_spend, ledger.process (*transaction, *block2).code);
}

TEST (ledger, fail_send_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	nano::keypair key2;
	auto block2 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key2.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *block2).code);
}

TEST (ledger, fail_open_old)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	auto block2 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	ASSERT_EQ (nano::process_result::old, ledger.process (*transaction, *block2).code);
}

TEST (ledger, fail_open_gap_source)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block2 = builder
				  .open ()
				  .source (1)
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::gap_source, result2.code);
}

TEST (ledger, fail_open_bad_signature)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	auto block2 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	block2->sign_zero ();
	ASSERT_EQ (nano::process_result::bad_signature, ledger.process (*transaction, *block2).code);
}

TEST (ledger, fail_open_fork_previous)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block3).code);
	auto block4 = builder
				  .open ()
				  .source (block2->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *block4).code);
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, fail_open_account_mismatch)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	nano::keypair badkey;
	auto block2 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (badkey.pub)
				  .sign (badkey.prv, badkey.pub)
				  .work (*pool.generate (badkey.pub))
				  .build ();
	ASSERT_NE (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, fail_receive_old)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block3).code);
	auto block4 = builder
				  .receive ()
				  .previous (block3->hash ())
				  .source (block2->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block4).code);
	ASSERT_EQ (nano::process_result::old, ledger.process (*transaction, *block4).code);
}

TEST (ledger, fail_receive_gap_source)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result2.code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result3 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::progress, result3.code);
	auto block4 = builder
				  .receive ()
				  .previous (block3->hash ())
				  .source (1)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	auto result4 = ledger.process (*transaction, *block4);
	ASSERT_EQ (nano::process_result::gap_source, result4.code);
}

TEST (ledger, fail_receive_overreceive)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result3 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result3.code);
	auto block3 = builder
				  .receive ()
				  .previous (block2->hash ())
				  .source (block1->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block2->hash ()))
				  .build ();
	auto result4 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::unreceivable, result4.code);
}

TEST (ledger, fail_receive_bad_signature)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result2.code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result3 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::progress, result3.code);
	auto block4 = builder
				  .receive ()
				  .previous (block3->hash ())
				  .source (block2->hash ())
				  .sign (nano::keypair ().prv, 0)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	auto result4 = ledger.process (*transaction, *block4);
	ASSERT_EQ (nano::process_result::bad_signature, result4.code);
}

TEST (ledger, fail_receive_gap_previous_opened)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result2.code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result3 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::progress, result3.code);
	auto block4 = builder
				  .receive ()
				  .previous (1)
				  .source (block2->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (nano::root (1)))
				  .build ();
	auto result4 = ledger.process (*transaction, *block4);
	ASSERT_EQ (nano::process_result::gap_previous, result4.code);
}

TEST (ledger, fail_receive_gap_previous_unopened)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result2.code);
	auto block3 = builder
				  .receive ()
				  .previous (1)
				  .source (block2->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (nano::root (1)))
				  .build ();
	auto result3 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::gap_previous, result3.code);
}

TEST (ledger, fail_receive_fork_previous)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result2.code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result3 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::progress, result3.code);
	nano::keypair key2;
	auto block4 = builder
				  .send ()
				  .previous (block3->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	auto result4 = ledger.process (*transaction, *block4);
	ASSERT_EQ (nano::process_result::progress, result4.code);
	auto block5 = builder
				  .receive ()
				  .previous (block3->hash ())
				  .source (block2->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	auto result5 = ledger.process (*transaction, *block5);
	ASSERT_EQ (nano::process_result::fork, result5.code);
}

TEST (ledger, fail_receive_received_source)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (key1.pub)
				  .balance (2)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto result1 = ledger.process (*transaction, *block1);
	ASSERT_EQ (nano::process_result::progress, result1.code);
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	auto result2 = ledger.process (*transaction, *block2);
	ASSERT_EQ (nano::process_result::progress, result2.code);
	auto block6 = builder
				  .send ()
				  .previous (block2->hash ())
				  .destination (key1.pub)
				  .balance (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block2->hash ()))
				  .build ();
	auto result6 = ledger.process (*transaction, *block6);
	ASSERT_EQ (nano::process_result::progress, result6.code);
	auto block3 = builder
				  .open ()
				  .source (block1->hash ())
				  .representative (1)
				  .account (key1.pub)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (key1.pub))
				  .build ();
	auto result3 = ledger.process (*transaction, *block3);
	ASSERT_EQ (nano::process_result::progress, result3.code);
	nano::keypair key2;
	auto block4 = builder
				  .send ()
				  .previous (block3->hash ())
				  .destination (key1.pub)
				  .balance (1)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	auto result4 = ledger.process (*transaction, *block4);
	ASSERT_EQ (nano::process_result::progress, result4.code);
	auto block5 = builder
				  .receive ()
				  .previous (block4->hash ())
				  .source (block2->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block4->hash ()))
				  .build ();
	auto result5 = ledger.process (*transaction, *block5);
	ASSERT_EQ (nano::process_result::progress, result5.code);
	auto block7 = builder
				  .receive ()
				  .previous (block3->hash ())
				  .source (block2->hash ())
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	auto result7 = ledger.process (*transaction, *block7);
	ASSERT_EQ (nano::process_result::fork, result7.code);
}

TEST (ledger, latest_empty)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::keypair key;
	auto transaction = store.tx_begin_read ();
	auto latest = ledger.latest (*transaction, key.pub);
	ASSERT_TRUE (latest.is_zero ());
}

TEST (ledger, latest_root)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key;
	ASSERT_EQ (key.pub, ledger.latest_root (*transaction, key.pub));
	auto hash1 = ledger.latest (*transaction, nano::dev::genesis_key.pub);
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (hash1)
				.destination (0)
				.balance (1)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (hash1))
				.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	ASSERT_EQ (send->hash (), ledger.latest_root (*transaction, nano::dev::genesis_key.pub));
}

TEST (ledger, change_representative_move_representation)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::keypair key1;
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis_key.pub));
	nano::block_builder builder;
	auto send = builder
				.send ()
				.previous (nano::dev::genesis->hash ())
				.destination (key1.pub)
				.balance (0)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (nano::dev::genesis->hash ()))
				.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis_key.pub));
	nano::keypair key2;
	auto change = builder
				  .change ()
				  .previous (send->hash ())
				  .representative (key2.pub)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (send->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change).code);
	nano::keypair key3;
	auto open = builder
				.open ()
				.source (send->hash ())
				.representative (key3.pub)
				.account (key1.pub)
				.sign (key1.prv, key1.pub)
				.work (*pool.generate (key1.pub))
				.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (key3.pub));
}

TEST (ledger, send_open_receive_rollback)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info info1;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
	nano::keypair key1;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (info1.head ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 50)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (info1.head ()))
				 .build ();
	auto return1 = ledger.process (*transaction, *send1);
	ASSERT_EQ (nano::process_result::progress, return1.code);
	auto send2 = builder
				 .send ()
				 .previous (send1->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	auto return2 = ledger.process (*transaction, *send2);
	ASSERT_EQ (nano::process_result::progress, return2.code);
	nano::keypair key2;
	auto open = builder
				.open ()
				.source (send2->hash ())
				.representative (key2.pub)
				.account (key1.pub)
				.sign (key1.prv, key1.pub)
				.work (*pool.generate (key1.pub))
				.build ();
	auto return4 = ledger.process (*transaction, *open);
	ASSERT_EQ (nano::process_result::progress, return4.code);
	auto receive = builder
				   .receive ()
				   .previous (open->hash ())
				   .source (send1->hash ())
				   .sign (key1.prv, key1.pub)
				   .work (*pool.generate (open->hash ()))
				   .build ();
	auto return5 = ledger.process (*transaction, *receive);
	ASSERT_EQ (nano::process_result::progress, return5.code);
	nano::keypair key3;
	ASSERT_EQ (100, ledger.weight (key2.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key3.pub));
	auto change1 = builder
				   .change ()
				   .previous (send2->hash ())
				   .representative (key3.pub)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (send2->hash ()))
				   .build ();
	auto return6 = ledger.process (*transaction, *change1);
	ASSERT_EQ (nano::process_result::progress, return6.code);
	ASSERT_EQ (100, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, ledger.weight (key3.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, receive->hash ()));
	ASSERT_EQ (50, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, ledger.weight (key3.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, open->hash ()));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, ledger.weight (key3.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, change1->hash ()));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (key3.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 100, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, send2->hash ()));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (key3.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 50, ledger.weight (nano::dev::genesis_key.pub));
	ASSERT_FALSE (ledger.rollback (*transaction, send1->hash ()));
	ASSERT_EQ (0, ledger.weight (key2.pub));
	ASSERT_EQ (0, ledger.weight (key3.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - 0, ledger.weight (nano::dev::genesis_key.pub));
}

TEST (ledger, bootstrap_rep_weight)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::account_info info1;
	nano::keypair key2;
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	{
		auto transaction = store.tx_begin_write ();
		ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
		nano::block_builder builder;
		auto send = builder
					.send ()
					.previous (info1.head ())
					.destination (key2.pub)
					.balance (std::numeric_limits<nano::uint128_t>::max () - 50)
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (info1.head ()))
					.build ();
		ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	}
	ASSERT_EQ (2, ledger.cache.block_count ());
	{
		ledger.set_bootstrap_weight_max_blocks (3);
		auto weights = ledger.get_bootstrap_weights ();
		weights[key2.pub] = 1000;
		ledger.set_bootstrap_weights (weights);
		ASSERT_EQ (1000, ledger.weight (key2.pub));
	}
	{
		auto transaction = store.tx_begin_write ();
		ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, info1));
		nano::block_builder builder;
		auto send = builder
					.send ()
					.previous (info1.head ())
					.destination (key2.pub)
					.balance (std::numeric_limits<nano::uint128_t>::max () - 100)
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (info1.head ()))
					.build ();
		ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
	}
	ASSERT_EQ (3, ledger.cache.block_count ());
	ASSERT_EQ (0, ledger.weight (key2.pub));
}

TEST (ledger, block_destination_source)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair dest;
	nano::uint128_t balance (nano::dev::constants.genesis_amount);
	balance -= nano::Gxrb_ratio;
	nano::block_builder builder;
	auto block1 = builder
				  .send ()
				  .previous (nano::dev::genesis->hash ())
				  .destination (dest.pub)
				  .balance (balance)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	balance -= nano::Gxrb_ratio;
	auto block2 = builder
				  .send ()
				  .previous (block1->hash ())
				  .destination (nano::dev::genesis->account ())
				  .balance (balance)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block1->hash ()))
				  .build ();
	balance += nano::Gxrb_ratio;
	auto block3 = builder
				  .receive ()
				  .previous (block2->hash ())
				  .source (block2->hash ())
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block2->hash ()))
				  .build ();
	balance -= nano::Gxrb_ratio;
	auto block4 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (block3->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (balance)
				  .link (dest.pub)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block3->hash ()))
				  .build ();
	balance -= nano::Gxrb_ratio;
	auto block5 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (block4->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (balance)
				  .link (nano::dev::genesis->account ())
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block4->hash ()))
				  .build ();
	balance += nano::Gxrb_ratio;
	auto block6 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (block5->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (balance)
				  .link (block5->hash ())
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (block5->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block1).code);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block2).code);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block3).code);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block4).code);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block5).code);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *block6).code);
	ASSERT_EQ (balance, ledger.balance (*transaction, block6->hash ()));
	ASSERT_EQ (dest.pub, ledger.block_destination (*transaction, *block1));
	ASSERT_TRUE (ledger.block_source (*transaction, *block1).is_zero ());
	ASSERT_EQ (nano::dev::genesis->account (), ledger.block_destination (*transaction, *block2));
	ASSERT_TRUE (ledger.block_source (*transaction, *block2).is_zero ());
	ASSERT_TRUE (ledger.block_destination (*transaction, *block3).is_zero ());
	ASSERT_EQ (block2->hash (), ledger.block_source (*transaction, *block3));
	ASSERT_EQ (dest.pub, ledger.block_destination (*transaction, *block4));
	ASSERT_TRUE (ledger.block_source (*transaction, *block4).is_zero ());
	ASSERT_EQ (nano::dev::genesis->account (), ledger.block_destination (*transaction, *block5));
	ASSERT_TRUE (ledger.block_source (*transaction, *block5).is_zero ());
	ASSERT_TRUE (ledger.block_destination (*transaction, *block6).is_zero ());
	ASSERT_EQ (block5->hash (), ledger.block_source (*transaction, *block6));
}

TEST (ledger, state_account)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_EQ (nano::dev::genesis->account (), ledger.account (*transaction, send1->hash ()));
}

TEST (ledger, state_send_receive)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_TRUE (store.pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (2, send2->sideband ().height ());
	ASSERT_TRUE (send2->sideband ().details ().is_send ());
	ASSERT_FALSE (send2->sideband ().details ().is_receive ());
	ASSERT_FALSE (send2->sideband ().details ().is_epoch ());
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, receive1->hash ()));
	auto receive2 = store.block ().get (*transaction, receive1->hash ());
	ASSERT_NE (nullptr, receive2);
	ASSERT_EQ (*receive1, *receive2);
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.balance (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_FALSE (store.pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
	ASSERT_EQ (3, receive2->sideband ().height ());
	ASSERT_FALSE (receive2->sideband ().details ().is_send ());
	ASSERT_TRUE (receive2->sideband ().details ().is_receive ());
	ASSERT_FALSE (receive2->sideband ().details ().is_epoch ());
}

TEST (ledger, state_receive)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, receive1->hash ()));
	auto receive2 = store.block ().get (*transaction, receive1->hash ());
	ASSERT_NE (nullptr, receive2);
	ASSERT_EQ (*receive1, *receive2);
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.balance (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (3, receive2->sideband ().height ());
	ASSERT_FALSE (receive2->sideband ().details ().is_send ());
	ASSERT_TRUE (receive2->sideband ().details ().is_receive ());
	ASSERT_FALSE (receive2->sideband ().details ().is_epoch ());
}

TEST (ledger, state_rep_change)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair rep;
	nano::block_builder builder;
	auto change1 = builder
				   .state ()
				   .account (nano::dev::genesis->account ())
				   .previous (nano::dev::genesis->hash ())
				   .representative (rep.pub)
				   .balance (nano::dev::constants.genesis_amount)
				   .link (0)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (nano::dev::genesis->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, change1->hash ()));
	auto change2 = store.block ().get (*transaction, change1->hash ());
	ASSERT_NE (nullptr, change2);
	ASSERT_EQ (*change1, *change2);
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.balance (*transaction, change1->hash ()));
	ASSERT_EQ (0, ledger.amount (*transaction, change1->hash ()));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (rep.pub));
	ASSERT_EQ (2, change2->sideband ().height ());
	ASSERT_FALSE (change2->sideband ().details ().is_send ());
	ASSERT_FALSE (change2->sideband ().details ().is_receive ());
	ASSERT_FALSE (change2->sideband ().details ().is_epoch ());
}

TEST (ledger, state_open)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_TRUE (store.pending ().exists (*transaction, nano::pending_key (destination.pub, send1->hash ())));
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_FALSE (store.pending ().exists (*transaction, nano::pending_key (destination.pub, send1->hash ())));
	ASSERT_TRUE (store.block ().exists (*transaction, open1->hash ()));
	auto open2 = store.block ().get (*transaction, open1->hash ());
	ASSERT_NE (nullptr, open2);
	ASSERT_EQ (*open1, *open2);
	ASSERT_EQ (nano::Gxrb_ratio, ledger.balance (*transaction, open1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, open1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (ledger.cache.account_count (), store.account ().count (*transaction));
	ASSERT_EQ (1, open2->sideband ().height ());
	ASSERT_FALSE (open2->sideband ().details ().is_send ());
	ASSERT_TRUE (open2->sideband ().details ().is_receive ());
	ASSERT_FALSE (open2->sideband ().details ().is_epoch ());
}

// Make sure old block types can't be inserted after a state block.
TEST (ledger, send_after_state_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto send2 = builder
				 .send ()
				 .previous (send1->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - (2 * nano::Gxrb_ratio))
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *send2).code);
}

// Make sure old block types can't be inserted after a state block.
TEST (ledger, receive_after_state_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto receive1 = builder
					.receive ()
					.previous (send1->hash ())
					.source (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *receive1).code);
}

// Make sure old block types can't be inserted after a state block.
TEST (ledger, change_after_state_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	nano::keypair rep;
	auto change1 = builder
				   .change ()
				   .previous (send1->hash ())
				   .representative (rep.pub)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (send1->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *change1).code);
}

TEST (ledger, state_unreceivable_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount)
					.link (1)
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::gap_source, ledger.process (*transaction, *receive1).code);
}

TEST (ledger, state_receive_bad_amount_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::balance_mismatch, ledger.process (*transaction, *receive1).code);
}

TEST (ledger, state_no_link_amount_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	nano::keypair rep;
	auto change1 = builder
				   .state ()
				   .account (nano::dev::genesis->account ())
				   .previous (send1->hash ())
				   .representative (rep.pub)
				   .balance (nano::dev::constants.genesis_amount)
				   .link (0)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (send1->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::balance_mismatch, ledger.process (*transaction, *change1).code);
}

TEST (ledger, state_receive_wrong_account_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	nano::keypair key;
	auto receive1 = builder
					.state ()
					.account (key.pub)
					.previous (0)
					.representative (nano::dev::genesis->account ())
					.balance (nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (key.prv, key.pub)
					.work (*pool.generate (key.pub))
					.build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *receive1).code);
}

TEST (ledger, state_open_state_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	auto open2 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *open2).code);
	ASSERT_EQ (open1->root (), open2->root ());
}

TEST (ledger, state_state_open_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	auto open2 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *open2).code);
	ASSERT_EQ (open1->root (), open2->root ());
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, state_open_previous_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (1)
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (1))
				 .build ();
	ASSERT_EQ (nano::process_result::gap_previous, ledger.process (*transaction, *open1).code);
}

TEST (ledger, state_open_source_fail)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (nano::dev::genesis->account ())
				 .balance (0)
				 .link (0)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::gap_source, ledger.process (*transaction, *open1).code);
}

TEST (ledger, state_send_change)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair rep;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (rep.pub)
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (rep.pub));
	ASSERT_EQ (2, send2->sideband ().height ());
	ASSERT_TRUE (send2->sideband ().details ().is_send ());
	ASSERT_FALSE (send2->sideband ().details ().is_receive ());
	ASSERT_FALSE (send2->sideband ().details ().is_epoch ());
}

TEST (ledger, state_receive_change)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.balance (*transaction, send1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	nano::keypair rep;
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (rep.pub)
					.balance (nano::dev::constants.genesis_amount)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, receive1->hash ()));
	auto receive2 = store.block ().get (*transaction, receive1->hash ());
	ASSERT_NE (nullptr, receive2);
	ASSERT_EQ (*receive1, *receive2);
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.balance (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, receive1->hash ()));
	ASSERT_EQ (0, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (rep.pub));
	ASSERT_EQ (3, receive2->sideband ().height ());
	ASSERT_FALSE (receive2->sideband ().details ().is_send ());
	ASSERT_TRUE (receive2->sideband ().details ().is_receive ());
	ASSERT_FALSE (receive2->sideband ().details ().is_epoch ());
}

TEST (ledger, state_open_old)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_EQ (nano::Gxrb_ratio, ledger.balance (*transaction, open1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, open1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
}

TEST (ledger, state_receive_old)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - (2 * nano::Gxrb_ratio))
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	auto receive1 = builder
					.receive ()
					.previous (open1->hash ())
					.source (send2->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (open1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_EQ (2 * nano::Gxrb_ratio, ledger.balance (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
}

TEST (ledger, state_rollback_send)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store.block ().exists (*transaction, send1->hash ()));
	auto send2 = store.block ().get (*transaction, send1->hash ());
	ASSERT_NE (nullptr, send2);
	ASSERT_EQ (*send1, *send2);
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	nano::pending_info info;
	ASSERT_FALSE (store.pending ().get (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ()), info));
	ASSERT_EQ (nano::dev::genesis->account (), info.source);
	ASSERT_EQ (nano::Gxrb_ratio, info.amount.number ());
	ASSERT_FALSE (ledger.rollback (*transaction, send1->hash ()));
	ASSERT_FALSE (store.block ().exists (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_FALSE (store.pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_TRUE (store.block ().successor (*transaction, nano::dev::genesis->hash ()).is_zero ());
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, state_rollback_receive)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (store.pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), receive1->hash ())));
	ASSERT_FALSE (ledger.rollback (*transaction, receive1->hash ()));
	nano::pending_info info;
	ASSERT_FALSE (store.pending ().get (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ()), info));
	ASSERT_EQ (nano::dev::genesis->account (), info.source);
	ASSERT_EQ (nano::Gxrb_ratio, info.amount.number ());
	ASSERT_FALSE (store.block ().exists (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, state_rollback_received_send)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (key.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto receive1 = builder
					.state ()
					.account (key.pub)
					.previous (0)
					.representative (key.pub)
					.balance (nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (key.prv, key.pub)
					.work (*pool.generate (key.pub))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (store.pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), receive1->hash ())));
	ASSERT_FALSE (ledger.rollback (*transaction, send1->hash ()));
	ASSERT_FALSE (store.pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_FALSE (store.block ().exists (*transaction, send1->hash ()));
	ASSERT_FALSE (store.block ().exists (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (0, ledger.account_balance (*transaction, key.pub));
	ASSERT_EQ (0, ledger.weight (key.pub));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, state_rep_change_rollback)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair rep;
	nano::block_builder builder;
	auto change1 = builder
				   .state ()
				   .account (nano::dev::genesis->account ())
				   .previous (nano::dev::genesis->hash ())
				   .representative (rep.pub)
				   .balance (nano::dev::constants.genesis_amount)
				   .link (0)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (nano::dev::genesis->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change1).code);
	ASSERT_FALSE (ledger.rollback (*transaction, change1->hash ()));
	ASSERT_FALSE (store.block ().exists (*transaction, change1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (0, ledger.weight (rep.pub));
}

TEST (ledger, state_open_rollback)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_FALSE (ledger.rollback (*transaction, open1->hash ()));
	ASSERT_FALSE (store.block ().exists (*transaction, open1->hash ()));
	ASSERT_EQ (0, ledger.account_balance (*transaction, destination.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	nano::pending_info info;
	ASSERT_FALSE (store.pending ().get (*transaction, nano::pending_key (destination.pub, send1->hash ()), info));
	ASSERT_EQ (nano::dev::genesis->account (), info.source);
	ASSERT_EQ (nano::Gxrb_ratio, info.amount.number ());
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, state_send_change_rollback)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair rep;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (rep.pub)
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_FALSE (ledger.rollback (*transaction, send1->hash ()));
	ASSERT_FALSE (store.block ().exists (*transaction, send1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (0, ledger.weight (rep.pub));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, state_receive_change_rollback)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	nano::keypair rep;
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send1->hash ())
					.representative (rep.pub)
					.balance (nano::dev::constants.genesis_amount)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (ledger.rollback (*transaction, receive1->hash ()));
	ASSERT_FALSE (store.block ().exists (*transaction, receive1->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.account_balance (*transaction, nano::dev::genesis->account ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (0, ledger.weight (rep.pub));
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, epoch_blocks_v1_general)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto epoch1 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (nano::dev::genesis->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	ASSERT_FALSE (epoch1->sideband ().details ().is_send ());
	ASSERT_FALSE (epoch1->sideband ().details ().is_receive ());
	ASSERT_TRUE (epoch1->sideband ().details ().is_epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, epoch1->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch1->sideband ().source_epoch ()); // Not used for epoch blocks
	auto epoch2 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (epoch1->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (epoch1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *epoch2).code);
	nano::account_info genesis_info;
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis->account (), genesis_info));
	ASSERT_EQ (genesis_info.epoch (), nano::epoch::epoch_1);
	ASSERT_FALSE (ledger.rollback (*transaction, epoch1->hash ()));
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis->account (), genesis_info));
	ASSERT_EQ (genesis_info.epoch (), nano::epoch::epoch_0);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis->account (), genesis_info));
	ASSERT_EQ (genesis_info.epoch (), nano::epoch::epoch_1);
	ASSERT_FALSE (epoch1->sideband ().details ().is_send ());
	ASSERT_FALSE (epoch1->sideband ().details ().is_receive ());
	ASSERT_TRUE (epoch1->sideband ().details ().is_epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, epoch1->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch1->sideband ().source_epoch ()); // Not used for epoch blocks
	auto change1 = builder
				   .change ()
				   .previous (epoch1->hash ())
				   .representative (nano::dev::genesis->account ())
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (epoch1->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *change1).code);
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (epoch1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (epoch1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (send1->sideband ().details ().is_send ());
	ASSERT_FALSE (send1->sideband ().details ().is_receive ());
	ASSERT_FALSE (send1->sideband ().details ().is_epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, send1->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, send1->sideband ().source_epoch ()); // Not used for send blocks
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *open1).code);
	auto epoch3 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (0)
				  .representative (nano::dev::genesis->account ())
				  .balance (0)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (destination.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::representative_mismatch, ledger.process (*transaction, *epoch3).code);
	auto epoch4 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (0)
				  .representative (0)
				  .balance (0)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (destination.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch4).code);
	ASSERT_FALSE (epoch4->sideband ().details ().is_send ());
	ASSERT_FALSE (epoch4->sideband ().details ().is_receive ());
	ASSERT_TRUE (epoch4->sideband ().details ().is_epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, epoch4->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch4->sideband ().source_epoch ()); // Not used for epoch blocks
	auto receive1 = builder
					.receive ()
					.previous (epoch4->hash ())
					.source (send1->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (epoch4->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *receive1).code);
	auto receive2 = builder
					.state ()
					.account (destination.pub)
					.previous (epoch4->hash ())
					.representative (destination.pub)
					.balance (nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (epoch4->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive2).code);
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().source_epoch ());
	ASSERT_EQ (0, ledger.balance (*transaction, epoch4->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.balance (*transaction, receive2->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, receive2->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.weight (destination.pub));
	ASSERT_FALSE (receive2->sideband ().details ().is_send ());
	ASSERT_TRUE (receive2->sideband ().details ().is_receive ());
	ASSERT_FALSE (receive2->sideband ().details ().is_epoch ());
}

TEST (ledger, epoch_blocks_v2_general)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto epoch1 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (nano::dev::genesis->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	// Trying to upgrade from epoch 0 to epoch 2. It is a requirement epoch upgrades are sequential unless the account is unopened
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *epoch1).code);
	// Set it to the first epoch and it should now succeed
	epoch1 = builder
			 .state ()
			 .account (nano::dev::genesis->account ())
			 .previous (nano::dev::genesis->hash ())
			 .representative (nano::dev::genesis->account ())
			 .balance (nano::dev::constants.genesis_amount)
			 .link (ledger.epoch_link (nano::epoch::epoch_1))
			 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
			 .work (epoch1->block_work ())
			 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	ASSERT_EQ (nano::epoch::epoch_1, epoch1->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch1->sideband ().source_epoch ()); // Not used for epoch blocks
	auto epoch2 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (epoch1->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (epoch1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch2).code);
	ASSERT_EQ (nano::epoch::epoch_2, epoch2->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch2->sideband ().source_epoch ()); // Not used for epoch blocks
	auto epoch3 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (epoch2->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (epoch2->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *epoch3).code);
	nano::account_info genesis_info;
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis->account (), genesis_info));
	ASSERT_EQ (genesis_info.epoch (), nano::epoch::epoch_2);
	ASSERT_FALSE (ledger.rollback (*transaction, epoch1->hash ()));
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis->account (), genesis_info));
	ASSERT_EQ (genesis_info.epoch (), nano::epoch::epoch_0);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	ASSERT_FALSE (ledger.store.account ().get (*transaction, nano::dev::genesis->account (), genesis_info));
	ASSERT_EQ (genesis_info.epoch (), nano::epoch::epoch_1);
	auto change1 = builder
				   .change ()
				   .previous (epoch1->hash ())
				   .representative (nano::dev::genesis->account ())
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (epoch1->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *change1).code);
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (epoch1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (epoch1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_EQ (nano::epoch::epoch_1, send1->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, send1->sideband ().source_epoch ()); // Not used for send blocks
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *open1).code);
	auto epoch4 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (0)
				  .representative (0)
				  .balance (0)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (destination.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch4).code);
	ASSERT_EQ (nano::epoch::epoch_1, epoch4->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch4->sideband ().source_epoch ()); // Not used for epoch blocks
	auto epoch5 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (epoch4->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (0)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (epoch4->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::representative_mismatch, ledger.process (*transaction, *epoch5).code);
	auto epoch6 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (epoch4->hash ())
				  .representative (0)
				  .balance (0)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (epoch4->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch6).code);
	ASSERT_EQ (nano::epoch::epoch_2, epoch6->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch6->sideband ().source_epoch ()); // Not used for epoch blocks
	auto receive1 = builder
					.receive ()
					.previous (epoch6->hash ())
					.source (send1->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (epoch6->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::block_position, ledger.process (*transaction, *receive1).code);
	auto receive2 = builder
					.state ()
					.account (destination.pub)
					.previous (epoch6->hash ())
					.representative (destination.pub)
					.balance (nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (epoch6->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive2).code);
	ASSERT_EQ (nano::epoch::epoch_2, receive2->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().source_epoch ());
	ASSERT_EQ (0, ledger.balance (*transaction, epoch6->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.balance (*transaction, receive2->hash ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.amount (*transaction, receive2->hash ()));
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio, ledger.weight (nano::dev::genesis->account ()));
	ASSERT_EQ (nano::Gxrb_ratio, ledger.weight (destination.pub));
}

TEST (ledger, epoch_blocks_receive_upgrade)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto epoch1 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (send1->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (send1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (epoch1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (epoch1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_EQ (nano::epoch::epoch_1, send2->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, send2->sideband ().source_epoch ()); // Not used for send blocks
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (destination.pub)
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (destination.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_EQ (nano::epoch::epoch_0, open1->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, open1->sideband ().source_epoch ());
	auto receive1 = builder
					.receive ()
					.previous (open1->hash ())
					.source (send2->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (open1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *receive1).code);
	auto receive2 = builder
					.state ()
					.account (destination.pub)
					.previous (open1->hash ())
					.representative (destination.pub)
					.balance (nano::Gxrb_ratio * 2)
					.link (send2->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (open1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive2).code);
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().source_epoch ());
	nano::account_info destination_info;
	ASSERT_FALSE (ledger.store.account ().get (*transaction, destination.pub, destination_info));
	ASSERT_EQ (destination_info.epoch (), nano::epoch::epoch_1);
	ASSERT_FALSE (ledger.rollback (*transaction, receive2->hash ()));
	ASSERT_FALSE (ledger.store.account ().get (*transaction, destination.pub, destination_info));
	ASSERT_EQ (destination_info.epoch (), nano::epoch::epoch_0);
	nano::pending_info pending_send2;
	ASSERT_FALSE (ledger.store.pending ().get (*transaction, nano::pending_key (destination.pub, send2->hash ()), pending_send2));
	ASSERT_EQ (nano::dev::genesis_key.pub, pending_send2.source);
	ASSERT_EQ (nano::Gxrb_ratio, pending_send2.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_1, pending_send2.epoch);
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive2).code);
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_1, receive2->sideband ().source_epoch ());
	ASSERT_FALSE (ledger.store.account ().get (*transaction, destination.pub, destination_info));
	ASSERT_EQ (destination_info.epoch (), nano::epoch::epoch_1);
	nano::keypair destination2;
	auto send3 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (receive2->hash ())
				 .representative (destination.pub)
				 .balance (nano::Gxrb_ratio)
				 .link (destination2.pub)
				 .sign (destination.prv, destination.pub)
				 .work (*pool.generate (receive2->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send3).code);
	auto open2 = builder
				 .open ()
				 .source (send3->hash ())
				 .representative (destination2.pub)
				 .account (destination2.pub)
				 .sign (destination2.prv, destination2.pub)
				 .work (*pool.generate (destination2.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *open2).code);
	// Upgrade to epoch 2 and send to destination. Try to create an open block from an epoch 2 source block.
	nano::keypair destination3;
	auto epoch2 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (send2->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (send2->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch2).code);
	auto send4 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (epoch2->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 3)
				 .link (destination3.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (epoch2->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send4).code);
	auto open3 = builder
				 .open ()
				 .source (send4->hash ())
				 .representative (destination3.pub)
				 .account (destination3.pub)
				 .sign (destination3.prv, destination3.pub)
				 .work (*pool.generate (destination3.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::unreceivable, ledger.process (*transaction, *open3).code);
	// Send it to an epoch 1 account
	auto send5 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send4->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 4)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send4->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send5).code);
	ASSERT_FALSE (ledger.store.account ().get (*transaction, destination.pub, destination_info));
	ASSERT_EQ (destination_info.epoch (), nano::epoch::epoch_1);
	auto receive3 = builder
					.state ()
					.account (destination.pub)
					.previous (send3->hash ())
					.representative (destination.pub)
					.balance (nano::Gxrb_ratio * 2)
					.link (send5->hash ())
					.sign (destination.prv, destination.pub)
					.work (*pool.generate (send3->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive3).code);
	ASSERT_EQ (nano::epoch::epoch_2, receive3->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_2, receive3->sideband ().source_epoch ());
	ASSERT_FALSE (ledger.store.account ().get (*transaction, destination.pub, destination_info));
	ASSERT_EQ (destination_info.epoch (), nano::epoch::epoch_2);
	// Upgrade an unopened account straight to epoch 2
	nano::keypair destination4;
	auto send6 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send5->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 5)
				 .link (destination4.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send5->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send6).code);
	auto epoch4 = builder
				  .state ()
				  .account (destination4.pub)
				  .previous (0)
				  .representative (0)
				  .balance (0)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (destination4.pub))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch4).code);
	ASSERT_EQ (nano::epoch::epoch_2, epoch4->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch4->sideband ().source_epoch ()); // Not used for epoch blocks
	ASSERT_EQ (store.account ().count (*transaction), ledger.cache.account_count ());
}

TEST (ledger, epoch_blocks_fork)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (nano::account{})
				 .balance (nano::dev::constants.genesis_amount)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto epoch1 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (nano::dev::genesis->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *epoch1).code);
	auto epoch2 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (nano::dev::genesis->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *epoch2).code);
	auto epoch3 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (send1->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (send1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch3).code);
	ASSERT_EQ (nano::epoch::epoch_1, epoch3->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch3->sideband ().source_epoch ()); // Not used for epoch state blocks
	auto epoch4 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (send1->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_2))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (send1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::fork, ledger.process (*transaction, *epoch2).code);
}

TEST (ledger, successor_epoch)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	auto open = builder
				.state ()
				.account (key1.pub)
				.previous (0)
				.representative (key1.pub)
				.balance (1)
				.link (send1->hash ())
				.sign (key1.prv, key1.pub)
				.work (*pool.generate (key1.pub))
				.build ();
	auto change = builder
				  .state ()
				  .account (key1.pub)
				  .previous (open->hash ())
				  .representative (key1.pub)
				  .balance (1)
				  .link (0)
				  .sign (key1.prv, key1.pub)
				  .work (*pool.generate (open->hash ()))
				  .build ();
	auto open_hash = open->hash ();
	auto send2 = builder
				 .send ()
				 .previous (send1->hash ())
				 .destination (reinterpret_cast<nano::account const &> (open_hash))
				 .balance (nano::dev::constants.genesis_amount - 2)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	auto epoch_open = builder
					  .state ()
					  .account (reinterpret_cast<nano::account const &> (open_hash))
					  .previous (0)
					  .representative (0)
					  .balance (0)
					  .link (node1.ledger.epoch_link (nano::epoch::epoch_1))
					  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					  .work (*pool.generate (open->hash ()))
					  .build ();
	auto transaction (node1.store.tx_begin_write ());
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send1).code);
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *open).code);
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *change).code);
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *send2).code);
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *epoch_open).code);
	ASSERT_EQ (*change, *node1.ledger.successor (*transaction, change->qualified_root ()));
	ASSERT_EQ (*epoch_open, *node1.ledger.successor (*transaction, epoch_open->qualified_root ()));
	ASSERT_EQ (nano::epoch::epoch_1, epoch_open->sideband ().details ().epoch ());
	ASSERT_EQ (nano::epoch::epoch_0, epoch_open->sideband ().source_epoch ()); // Not used for epoch state blocks
}

TEST (ledger, epoch_open_pending)
{
	nano::block_builder builder{};
	nano::test::system system{ 1 };
	auto & node1 = *system.nodes[0];
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1{};
	auto epoch_open = builder.state ()
					  .account (key1.pub)
					  .previous (0)
					  .representative (0)
					  .balance (0)
					  .link (node1.ledger.epoch_link (nano::epoch::epoch_1))
					  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					  .work (*pool.generate (key1.pub))
					  .build_shared ();
	auto process_result = node1.ledger.process (*node1.store.tx_begin_write (), *epoch_open);
	ASSERT_EQ (nano::process_result::gap_epoch_open_pending, process_result.code);
	ASSERT_EQ (nano::signature_verification::valid_epoch, process_result.verified);
	node1.block_processor.add (epoch_open);
	// Waits for the block to get saved in the database
	ASSERT_TIMELY (10s, 1 == node1.unchecked.count (*node1.store.tx_begin_read ()));
	ASSERT_FALSE (node1.ledger.block_or_pruned_exists (epoch_open->hash ()));
	// Open block should be inserted into unchecked
	auto blocks = node1.unchecked.get (*node1.store.tx_begin_read (), nano::hash_or_account (epoch_open->account ()).hash);
	ASSERT_EQ (blocks.size (), 1);
	ASSERT_EQ (blocks[0].get_block ()->full_hash (), epoch_open->full_hash ());
	ASSERT_EQ (blocks[0].get_verified (), nano::signature_verification::valid_epoch);
	// New block to process epoch open
	auto send1 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build_shared ();
	node1.block_processor.add (send1);
	ASSERT_TIMELY (10s, node1.ledger.block_or_pruned_exists (epoch_open->hash ()));
}

TEST (ledger, block_hash_account_conflict)
{
	nano::block_builder builder;
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair key1;
	nano::keypair key2;
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };

	/*
	 * Generate a send block whose destination is a block hash already
	 * in the ledger and not an account
	 */
	auto send1 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build_shared ();

	auto receive1 = builder.state ()
					.account (key1.pub)
					.previous (0)
					.representative (nano::dev::genesis->account ())
					.balance (100)
					.link (send1->hash ())
					.sign (key1.prv, key1.pub)
					.work (*pool.generate (key1.pub))
					.build_shared ();

	/*
	 * Note that the below link is a block hash when this is intended
	 * to represent a send state block. This can generally never be
	 * received , except by epoch blocks, which can sign an open block
	 * for arbitrary accounts.
	 */
	auto send2 = builder.state ()
				 .account (key1.pub)
				 .previous (receive1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (90)
				 .link (receive1->hash ())
				 .sign (key1.prv, key1.pub)
				 .work (*pool.generate (receive1->hash ()))
				 .build_shared ();

	/*
	 * Generate an epoch open for the account with the same value as the block hash
	 */
	auto receive1_hash = receive1->hash ();
	auto open_epoch1 = builder.state ()
					   .account (reinterpret_cast<nano::account const &> (receive1_hash))
					   .previous (0)
					   .representative (0)
					   .balance (0)
					   .link (node1.ledger.epoch_link (nano::epoch::epoch_1))
					   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					   .work (*pool.generate (receive1->hash ()))
					   .build_shared ();

	node1.work_generate_blocking (*send1);
	node1.work_generate_blocking (*receive1);
	node1.work_generate_blocking (*send2);
	node1.work_generate_blocking (*open_epoch1);
	ASSERT_EQ (nano::process_result::progress, node1.process (*send1).code);
	ASSERT_EQ (nano::process_result::progress, node1.process (*receive1).code);
	ASSERT_EQ (nano::process_result::progress, node1.process (*send2).code);
	ASSERT_EQ (nano::process_result::progress, node1.process (*open_epoch1).code);
	nano::test::blocks_confirm (node1, { send1, receive1, send2, open_epoch1 });
	auto election1 = node1.active.election (send1->qualified_root ());
	ASSERT_NE (nullptr, election1);
	auto election2 = node1.active.election (receive1->qualified_root ());
	ASSERT_NE (nullptr, election2);
	auto election3 = node1.active.election (send2->qualified_root ());
	ASSERT_NE (nullptr, election3);
	auto election4 = node1.active.election (open_epoch1->qualified_root ());
	ASSERT_NE (nullptr, election4);
	auto winner1 (election1->winner ());
	auto winner2 (election2->winner ());
	auto winner3 (election3->winner ());
	auto winner4 (election4->winner ());
	ASSERT_EQ (*send1, *winner1);
	ASSERT_EQ (*receive1, *winner2);
	ASSERT_EQ (*send2, *winner3);
	ASSERT_EQ (*open_epoch1, *winner4);
}

TEST (ledger, could_fit)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair destination;
	// Test legacy and state change blocks could_fit
	nano::block_builder builder;
	auto change1 = builder
				   .change ()
				   .previous (nano::dev::genesis->hash ())
				   .representative (nano::dev::genesis->account ())
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (nano::dev::genesis->hash ()))
				   .build ();
	auto change2 = builder
				   .state ()
				   .account (nano::dev::genesis->account ())
				   .previous (nano::dev::genesis->hash ())
				   .representative (nano::dev::genesis->account ())
				   .balance (nano::dev::constants.genesis_amount)
				   .link (0)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (nano::dev::genesis->hash ()))
				   .build ();
	ASSERT_TRUE (ledger.could_fit (*transaction, *change1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *change2));
	// Test legacy and state send
	nano::keypair key1;
	auto send1 = builder
				 .send ()
				 .previous (change1->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 1)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (change1->hash ()))
				 .build ();
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (change1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 1)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (change1->hash ()))
				 .build ();
	ASSERT_FALSE (ledger.could_fit (*transaction, *send1));
	ASSERT_FALSE (ledger.could_fit (*transaction, *send2));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change1).code);
	ASSERT_TRUE (ledger.could_fit (*transaction, *change1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *change2));
	ASSERT_TRUE (ledger.could_fit (*transaction, *send1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *send2));
	// Test legacy and state open
	auto open1 = builder
				 .open ()
				 .source (send2->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (key1.pub)
				 .sign (key1.prv, key1.pub)
				 .work (*pool.generate (key1.pub))
				 .build ();
	auto open2 = builder
				 .state ()
				 .account (key1.pub)
				 .previous (0)
				 .representative (nano::dev::genesis->account ())
				 .balance (1)
				 .link (send2->hash ())
				 .sign (key1.prv, key1.pub)
				 .work (*pool.generate (key1.pub))
				 .build ();
	ASSERT_FALSE (ledger.could_fit (*transaction, *open1));
	ASSERT_FALSE (ledger.could_fit (*transaction, *open2));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_TRUE (ledger.could_fit (*transaction, *send1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *send2));
	ASSERT_TRUE (ledger.could_fit (*transaction, *open1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *open2));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_TRUE (ledger.could_fit (*transaction, *open1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *open2));
	// Create another send to receive
	auto send3 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send2->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 2)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send2->hash ()))
				 .build ();
	// Test legacy and state receive
	auto receive1 = builder
					.receive ()
					.previous (open1->hash ())
					.source (send3->hash ())
					.sign (key1.prv, key1.pub)
					.work (*pool.generate (open1->hash ()))
					.build ();
	auto receive2 = builder
					.state ()
					.account (key1.pub)
					.previous (open1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (2)
					.link (send3->hash ())
					.sign (key1.prv, key1.pub)
					.work (*pool.generate (open1->hash ()))
					.build ();
	ASSERT_FALSE (ledger.could_fit (*transaction, *receive1));
	ASSERT_FALSE (ledger.could_fit (*transaction, *receive2));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send3).code);
	ASSERT_TRUE (ledger.could_fit (*transaction, *receive1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *receive2));
	// Test epoch (state)
	auto epoch1 = builder
				  .state ()
				  .account (key1.pub)
				  .previous (receive1->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (2)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (receive1->hash ()))
				  .build ();
	ASSERT_FALSE (ledger.could_fit (*transaction, *epoch1));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_TRUE (ledger.could_fit (*transaction, *receive1));
	ASSERT_TRUE (ledger.could_fit (*transaction, *receive2));
	ASSERT_TRUE (ledger.could_fit (*transaction, *epoch1));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	ASSERT_TRUE (ledger.could_fit (*transaction, *epoch1));
}

TEST (ledger, unchecked_epoch)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (destination.pub)
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*open1);
	auto epoch1 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (open1->hash ())
				  .representative (destination.pub)
				  .balance (nano::Gxrb_ratio)
				  .link (node1.ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (0)
				  .build_shared ();
	node1.work_generate_blocking (*epoch1);
	node1.block_processor.add (epoch1);
	{
		// Waits for the epoch1 block to pass through block_processor and unchecked.put queues
		ASSERT_TIMELY (10s, 1 == node1.unchecked.count (*node1.store.tx_begin_read ()));
		auto blocks = node1.unchecked.get (*node1.store.tx_begin_read (), epoch1->previous ());
		ASSERT_EQ (blocks.size (), 1);
		ASSERT_EQ (blocks[0].get_verified (), nano::signature_verification::valid_epoch);
	}
	node1.block_processor.add (send1);
	node1.block_processor.add (open1);
	ASSERT_TIMELY (5s, node1.store.block ().exists (*node1.store.tx_begin_read (), epoch1->hash ()));
	{
		// Waits for the last blocks to pass through block_processor and unchecked.put queues
		ASSERT_TIMELY (10s, 0 == node1.unchecked.count (*node1.store.tx_begin_read ()));
		nano::account_info info{};
		ASSERT_FALSE (node1.store.account ().get (*node1.store.tx_begin_read (), destination.pub, info));
		ASSERT_EQ (info.epoch (), nano::epoch::epoch_1);
	}
}

TEST (ledger, unchecked_epoch_invalid)
{
	nano::test::system system;
	nano::node_config node_config (nano::test::get_available_port (), system.logging);
	node_config.frontiers_confirmation = nano::frontiers_confirmation_mode::disabled;
	auto & node1 (*system.add_node (node_config));
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto open1 = builder
				 .state ()
				 .account (destination.pub)
				 .previous (0)
				 .representative (destination.pub)
				 .balance (nano::Gxrb_ratio)
				 .link (send1->hash ())
				 .sign (destination.prv, destination.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*open1);
	// Epoch block with account own signature
	auto epoch1 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (open1->hash ())
				  .representative (destination.pub)
				  .balance (nano::Gxrb_ratio)
				  .link (node1.ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (destination.prv, destination.pub)
				  .work (0)
				  .build_shared ();
	node1.work_generate_blocking (*epoch1);
	// Pseudo epoch block (send subtype, destination - epoch link)
	auto epoch2 = builder
				  .state ()
				  .account (destination.pub)
				  .previous (open1->hash ())
				  .representative (destination.pub)
				  .balance (nano::Gxrb_ratio - 1)
				  .link (node1.ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (destination.prv, destination.pub)
				  .work (0)
				  .build_shared ();
	node1.work_generate_blocking (*epoch2);
	node1.block_processor.add (epoch1);
	node1.block_processor.add (epoch2);
	{
		// Waits for the last blocks to pass through block_processor and unchecked.put queues
		ASSERT_TIMELY (10s, 2 == node1.unchecked.count (*node1.store.tx_begin_read ()));
		auto blocks = node1.unchecked.get (*node1.store.tx_begin_read (), epoch1->previous ());
		ASSERT_EQ (blocks.size (), 2);
		ASSERT_EQ (blocks[0].get_verified (), nano::signature_verification::valid);
		ASSERT_EQ (blocks[1].get_verified (), nano::signature_verification::valid);
	}
	node1.block_processor.add (send1);
	node1.block_processor.add (open1);
	// Waits for the last blocks to pass through block_processor and unchecked.put queues
	ASSERT_TIMELY (10s, node1.store.block ().exists (*node1.store.tx_begin_read (), epoch2->hash ()));
	{
		auto transaction = node1.store.tx_begin_read ();
		ASSERT_FALSE (node1.store.block ().exists (*transaction, epoch1->hash ()));
		auto unchecked_count = node1.unchecked.count (*transaction);
		ASSERT_EQ (unchecked_count, 0);
		ASSERT_EQ (unchecked_count, node1.unchecked.count (*transaction));
		nano::account_info info{};
		ASSERT_FALSE (node1.store.account ().get (*transaction, destination.pub, info));
		ASSERT_NE (info.epoch (), nano::epoch::epoch_1);
		auto epoch2_store = node1.store.block ().get (*transaction, epoch2->hash ());
		ASSERT_NE (nullptr, epoch2_store);
		ASSERT_EQ (nano::epoch::epoch_0, epoch2_store->sideband ().details ().epoch ());
		ASSERT_TRUE (epoch2_store->sideband ().details ().is_send ());
		ASSERT_FALSE (epoch2_store->sideband ().details ().is_epoch ());
		ASSERT_FALSE (epoch2_store->sideband ().details ().is_receive ());
	}
}

TEST (ledger, unchecked_open)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::keypair destination;
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (destination.pub)
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*open1);
	// Invalid signature for open block
	auto open2 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis_key.pub)
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*open2);
	auto sig{ open2->block_signature () };
	sig.bytes[0] ^= 1;
	open2->signature_set (sig);
	node1.block_processor.add (open1);
	node1.block_processor.add (open2);
	{
		// Waits for the last blocks to pass through block_processor and unchecked.put queues
		ASSERT_TIMELY (10s, 1 == node1.unchecked.count (*node1.store.tx_begin_read ()));
		auto blocks = node1.unchecked.get (*node1.store.tx_begin_read (), open1->source ());
		ASSERT_EQ (blocks.size (), 1);
		ASSERT_EQ (blocks[0].get_verified (), nano::signature_verification::valid);
	}
	node1.block_processor.add (send1);
	// Waits for the send1 block to pass through block_processor and unchecked.put queues
	ASSERT_TIMELY (10s, node1.store.block ().exists (*node1.store.tx_begin_read (), open1->hash ()));
	ASSERT_EQ (0, node1.unchecked.count (*node1.store.tx_begin_read ()));
}

TEST (ledger, unchecked_receive)
{
	nano::test::system system{ 1 };
	auto & node1 = *system.nodes[0];
	nano::keypair destination{};
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send1);
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 2 * nano::Gxrb_ratio)
				 .link (destination.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*send2);
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (destination.pub)
				 .account (destination.pub)
				 .sign (destination.prv, destination.pub)
				 .work (0)
				 .build_shared ();
	node1.work_generate_blocking (*open1);
	auto receive1 = builder
					.receive ()
					.previous (open1->hash ())
					.source (send2->hash ())
					.sign (destination.prv, destination.pub)
					.work (0)
					.build_shared ();
	node1.work_generate_blocking (*receive1);
	node1.block_processor.add (send1);
	node1.block_processor.add (receive1);
	auto check_block_is_listed = [&] (nano::transaction const & transaction_a, nano::block_hash const & block_hash_a) {
		return !node1.unchecked.get (transaction_a, block_hash_a).empty ();
	};
	// Previous block for receive1 is unknown, signature cannot be validated
	{
		// Waits for the last blocks to pass through block_processor and unchecked.put queues
		ASSERT_TIMELY (15s, check_block_is_listed (*node1.store.tx_begin_read (), receive1->previous ()));
		auto blocks = node1.unchecked.get (*node1.store.tx_begin_read (), receive1->previous ());
		ASSERT_EQ (blocks.size (), 1);
		ASSERT_EQ (blocks[0].get_verified (), nano::signature_verification::unknown);
	}
	// Waits for the open1 block to pass through block_processor and unchecked.put queues
	node1.block_processor.add (open1);
	ASSERT_TIMELY (15s, check_block_is_listed (*node1.store.tx_begin_read (), receive1->source ()));
	// Previous block for receive1 is known, signature was validated
	{
		auto transaction = node1.store.tx_begin_read ();
		auto blocks (node1.unchecked.get (*transaction, receive1->source ()));
		ASSERT_EQ (blocks.size (), 1);
		ASSERT_EQ (blocks[0].get_verified (), nano::signature_verification::valid);
	}
	node1.block_processor.add (send2);
	ASSERT_TIMELY (10s, node1.store.block ().exists (*node1.store.tx_begin_read (), receive1->hash ()));
	ASSERT_EQ (0, node1.unchecked.count (*node1.store.tx_begin_read ()));
}

TEST (ledger, confirmation_height_not_updated)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::account_info account_info;
	ASSERT_FALSE (store.account ().get (*transaction, nano::dev::genesis_key.pub, account_info));
	nano::keypair key;
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (account_info.head ())
				 .destination (key.pub)
				 .balance (50)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (account_info.head ()))
				 .build ();
	nano::confirmation_height_info confirmation_height_info;
	ASSERT_FALSE (store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), confirmation_height_info));
	ASSERT_EQ (1, confirmation_height_info.height ());
	ASSERT_EQ (nano::dev::genesis->hash (), confirmation_height_info.frontier ());
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_FALSE (store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), confirmation_height_info));
	ASSERT_EQ (1, confirmation_height_info.height ());
	ASSERT_EQ (nano::dev::genesis->hash (), confirmation_height_info.frontier ());
	auto open1 = builder
				 .open ()
				 .source (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (key.pub)
				 .sign (key.prv, key.pub)
				 .work (*pool.generate (key.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_TRUE (store.confirmation_height ().get (*transaction, key.pub, confirmation_height_info));
	ASSERT_EQ (0, confirmation_height_info.height ());
	ASSERT_EQ (nano::block_hash (0), confirmation_height_info.frontier ());
}

TEST (ledger, zero_rep)
{
	nano::test::system system (1);
	auto & node1 (*system.nodes[0]);
	nano::block_builder builder;
	auto block1 = builder.state ()
				  .account (nano::dev::genesis_key.pub)
				  .previous (nano::dev::genesis->hash ())
				  .representative (0)
				  .balance (nano::dev::constants.genesis_amount)
				  .link (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*system.work.generate (nano::dev::genesis->hash ()))
				  .build ();
	auto transaction (node1.store.tx_begin_write ());
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *block1).code);
	ASSERT_EQ (0, node1.ledger.cache.rep_weights ().representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (nano::dev::constants.genesis_amount, node1.ledger.cache.rep_weights ().representation_get (0));
	auto block2 = builder.state ()
				  .account (nano::dev::genesis_key.pub)
				  .previous (block1->hash ())
				  .representative (nano::dev::genesis_key.pub)
				  .balance (nano::dev::constants.genesis_amount)
				  .link (0)
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*system.work.generate (block1->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, node1.ledger.process (*transaction, *block2).code);
	ASSERT_EQ (nano::dev::constants.genesis_amount, node1.ledger.cache.rep_weights ().representation_get (nano::dev::genesis_key.pub));
	ASSERT_EQ (0, node1.ledger.cache.rep_weights ().representation_get (0));
}

TEST (ledger, work_validation)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto gen = nano::dev::genesis_key;
	nano::keypair key;

	// With random work the block doesn't pass, then modifies the block with sufficient work and ensures a correct result
	auto process_block = [&store, &ledger, &pool] (nano::block & block_a, nano::block_details const details_a) {
		auto threshold = nano::dev::network_params.work.threshold (block_a.work_version (), details_a);
		// Rarely failed with random work, so modify until it doesn't have enough difficulty
		while (nano::dev::network_params.work.difficulty (block_a) >= threshold)
		{
			block_a.block_work_set (block_a.block_work () + 1);
		}
		EXPECT_EQ (nano::process_result::insufficient_work, ledger.process (*store.tx_begin_write (), block_a).code);
		block_a.block_work_set (*pool.generate (block_a.root (), threshold));
		EXPECT_EQ (nano::process_result::progress, ledger.process (*store.tx_begin_write (), block_a).code);
	};

	std::error_code ec;

	auto send = *builder.send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (gen.pub)
				 .balance (nano::dev::constants.genesis_amount - 1)
				 .sign (gen.prv, gen.pub)
				 .work (0)
				 .build (ec);
	ASSERT_FALSE (ec);

	auto receive = *builder.receive ()
					.previous (send.hash ())
					.source (send.hash ())
					.sign (gen.prv, gen.pub)
					.work (0)
					.build (ec);
	ASSERT_FALSE (ec);

	auto change = *builder.change ()
				   .previous (receive.hash ())
				   .representative (key.pub)
				   .sign (gen.prv, gen.pub)
				   .work (0)
				   .build (ec);
	ASSERT_FALSE (ec);

	auto state = *builder.state ()
				  .account (gen.pub)
				  .previous (change.hash ())
				  .representative (gen.pub)
				  .balance (nano::dev::constants.genesis_amount - 1)
				  .link (key.pub)
				  .sign (gen.prv, gen.pub)
				  .work (0)
				  .build (ec);
	ASSERT_FALSE (ec);

	auto open = *builder.open ()
				 .account (key.pub)
				 .source (state.hash ())
				 .representative (key.pub)
				 .sign (key.prv, key.pub)
				 .work (0)
				 .build (ec);
	ASSERT_FALSE (ec);

	auto epoch = *builder.state ()
				  .account (key.pub)
				  .previous (open.hash ())
				  .balance (1)
				  .representative (key.pub)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (gen.prv, gen.pub)
				  .work (0)
				  .build (ec);
	ASSERT_FALSE (ec);

	process_block (send, {});
	process_block (receive, {});
	process_block (change, {});
	process_block (state, nano::block_details (nano::epoch::epoch_0, true, false, false));
	process_block (open, {});
	process_block (epoch, nano::block_details (nano::epoch::epoch_1, false, false, true));
}

TEST (ledger, dependents_confirmed)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::block_builder builder;
	ASSERT_TRUE (ledger.dependents_confirmed (*transaction, *nano::dev::genesis));
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	auto send1 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build_shared ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (ledger.dependents_confirmed (*transaction, *send1));
	auto send2 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 200)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build_shared ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_FALSE (ledger.dependents_confirmed (*transaction, *send2));
	auto receive1 = builder.state ()
					.account (key1.pub)
					.previous (0)
					.representative (nano::dev::genesis->account ())
					.balance (100)
					.link (send1->hash ())
					.sign (key1.prv, key1.pub)
					.work (*pool.generate (key1.pub))
					.build_shared ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (ledger.dependents_confirmed (*transaction, *receive1));
	nano::confirmation_height_info height;
	ASSERT_FALSE (ledger.store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), height));
	height = nano::confirmation_height_info (height.height () + 1, height.frontier ());
	ledger.store.confirmation_height ().put (*transaction, nano::dev::genesis->account (), height);
	ASSERT_TRUE (ledger.dependents_confirmed (*transaction, *receive1));
	auto receive2 = builder.state ()
					.account (key1.pub)
					.previous (receive1->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (200)
					.link (send2->hash ())
					.sign (key1.prv, key1.pub)
					.work (*pool.generate (receive1->hash ()))
					.build_shared ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive2).code);
	ASSERT_FALSE (ledger.dependents_confirmed (*transaction, *receive2));
	ASSERT_TRUE (ledger.store.confirmation_height ().get (*transaction, key1.pub, height));
	height = nano::confirmation_height_info (height.height () + 1, height.frontier ());
	ledger.store.confirmation_height ().put (*transaction, key1.pub, height);
	ASSERT_FALSE (ledger.dependents_confirmed (*transaction, *receive2));
	ASSERT_FALSE (ledger.store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), height));
	height = nano::confirmation_height_info (height.height () + 1, height.frontier ());
	ledger.store.confirmation_height ().put (*transaction, nano::dev::genesis->account (), height);
	ASSERT_TRUE (ledger.dependents_confirmed (*transaction, *receive2));
}

TEST (ledger, dependents_confirmed_pruning)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_FALSE (store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::block_builder builder;
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	auto send1 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build_shared ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	auto send2 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 200)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build_shared ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	nano::confirmation_height_info height;
	ASSERT_FALSE (ledger.store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), height));
	height = nano::confirmation_height_info (3, height.frontier ());
	ledger.store.confirmation_height ().put (*transaction, nano::dev::genesis->account (), height);
	ASSERT_TRUE (ledger.block_confirmed (*transaction, send1->hash ()));
	ASSERT_EQ (2, ledger.pruning_action (*transaction, send2->hash (), 1));
	auto receive1 = builder.state ()
					.account (key1.pub)
					.previous (0)
					.representative (nano::dev::genesis->account ())
					.balance (100)
					.link (send1->hash ())
					.sign (key1.prv, key1.pub)
					.work (*pool.generate (key1.pub))
					.build_shared ();
	ASSERT_TRUE (ledger.dependents_confirmed (*transaction, *receive1));
}

TEST (ledger, block_confirmed)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto transaction = store.tx_begin_write ();
	nano::block_builder builder;
	ASSERT_TRUE (ledger.block_confirmed (*transaction, nano::dev::genesis->hash ()));
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::keypair key1;
	auto send1 = builder.state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 100)
				 .link (key1.pub)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	// Must be safe against non-existing blocks
	ASSERT_FALSE (ledger.block_confirmed (*transaction, send1->hash ()));
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_FALSE (ledger.block_confirmed (*transaction, send1->hash ()));
	nano::confirmation_height_info height;
	ASSERT_FALSE (ledger.store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), height));
	height = nano::confirmation_height_info (height.height () + 1, height.frontier ());
	ledger.store.confirmation_height ().put (*transaction, nano::dev::genesis->account (), height);
	ASSERT_TRUE (ledger.block_confirmed (*transaction, send1->hash ()));
}

TEST (ledger, cache)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto & stats = ctx.stats ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;

	size_t const total = 100;

	// Check existing ledger (incremental cache update) and reload on a new ledger
	for (size_t i (0); i < total; ++i)
	{
		auto account_count = 1 + i;
		auto block_count = 1 + 2 * (i + 1) - 2;
		auto cemented_count = 1 + 2 * (i + 1) - 2;
		auto genesis_weight = nano::dev::constants.genesis_amount - i;
		auto pruned_count = i;

		auto cache_check = [&, i] (nano::ledger_cache & cache_a) {
			ASSERT_EQ (account_count, cache_a.account_count ());
			ASSERT_EQ (block_count, cache_a.block_count ());
			ASSERT_EQ (cemented_count, cache_a.cemented_count ());
			ASSERT_EQ (genesis_weight, cache_a.rep_weights ().representation_get (nano::dev::genesis->account ()));
			ASSERT_EQ (pruned_count, cache_a.pruned_count ());
		};

		nano::keypair key;
		auto const latest = ledger.latest (*store.tx_begin_read (), nano::dev::genesis->account ());
		auto send = builder.state ()
					.account (nano::dev::genesis->account ())
					.previous (latest)
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount - (i + 1))
					.link (key.pub)
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (latest))
					.build ();
		auto open = builder.state ()
					.account (key.pub)
					.previous (0)
					.representative (key.pub)
					.balance (1)
					.link (send->hash ())
					.sign (key.prv, key.pub)
					.work (*pool.generate (key.pub))
					.build ();
		{
			auto transaction (store.tx_begin_write ());
			ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
		}

		++block_count;
		--genesis_weight;
		cache_check (ledger.cache);
		nano::ledger ledger2{ store, stats, nano::dev::constants };
		cache_check (ledger2.cache);

		{
			auto transaction (store.tx_begin_write ());
			ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open).code);
		}

		++block_count;
		++account_count;
		cache_check (ledger.cache);
		nano::ledger ledger3{ store, stats, nano::dev::constants };
		cache_check (ledger3.cache);

		{
			auto transaction (store.tx_begin_write ());
			nano::confirmation_height_info height;
			ASSERT_FALSE (ledger.store.confirmation_height ().get (*transaction, nano::dev::genesis->account (), height));
			height = nano::confirmation_height_info (height.height () + 1, send->hash ());
			ledger.store.confirmation_height ().put (*transaction, nano::dev::genesis->account (), height);
			ASSERT_TRUE (ledger.block_confirmed (*transaction, send->hash ()));
			ledger.cache.add_cemented (1);
		}

		++cemented_count;
		cache_check (ledger.cache);
		nano::ledger ledger4{ store, stats, nano::dev::constants };
		cache_check (ledger4.cache);

		{
			auto transaction (store.tx_begin_write ());
			nano::confirmation_height_info height;
			ledger.store.confirmation_height ().get (*transaction, key.pub, height);
			height = nano::confirmation_height_info (height.height () + 1, open->hash ());
			ledger.store.confirmation_height ().put (*transaction, key.pub, height);
			ASSERT_TRUE (ledger.block_confirmed (*transaction, open->hash ()));
			ledger.cache.add_cemented (1);
		}

		++cemented_count;
		cache_check (ledger.cache);
		nano::ledger ledger5{ store, stats, nano::dev::constants };
		cache_check (ledger5.cache);

		{
			auto transaction (store.tx_begin_write ());
			ledger.store.pruned ().put (*transaction, open->hash ());
			ledger.cache.add_pruned (1);
		}
		++pruned_count;
		cache_check (ledger.cache);
		nano::ledger ledger6{ store, stats, nano::dev::constants };
		cache_check (ledger6.cache);
	}
}

TEST (ledger, pruning_action)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send1->hash ()));
	auto send1_stored (store->block ().get (*transaction, send1->hash ()));
	ASSERT_NE (nullptr, send1_stored);
	ASSERT_EQ (*send1, *send1_stored);
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Pruning action
	ASSERT_EQ (1, ledger.pruning_action (*transaction, send1->hash (), 1));
	ASSERT_EQ (0, ledger.pruning_action (*transaction, nano::dev::genesis->hash (), 1));
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (ledger.block_or_pruned_exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Receiving pruned block
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send2->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send2->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_TRUE (store->block ().exists (*transaction, receive1->hash ()));
	auto receive1_stored (store->block ().get (*transaction, receive1->hash ()));
	ASSERT_NE (nullptr, receive1_stored);
	ASSERT_EQ (*receive1, *receive1_stored);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (4, receive1_stored->sideband ().height ());
	ASSERT_FALSE (receive1_stored->sideband ().details ().is_send ());
	ASSERT_TRUE (receive1_stored->sideband ().details ().is_receive ());
	ASSERT_FALSE (receive1_stored->sideband ().details ().is_epoch ());
	// Middle block pruning
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	ASSERT_EQ (1, ledger.pruning_action (*transaction, send2->hash (), 1));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send2->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, send2->hash ()));
	ASSERT_EQ (store->account ().count (*transaction), ledger.cache.account_count ());
	ASSERT_EQ (store->pruned ().count (*transaction), ledger.cache.pruned_count ());
	ASSERT_EQ (store->block ().count (*transaction), ledger.cache.block_count () - ledger.cache.pruned_count ());
}

TEST (ledger, pruning_large_chain)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	size_t send_receive_pairs (20);
	auto last_hash (nano::dev::genesis->hash ());
	nano::block_builder builder;
	for (auto i (0); i < send_receive_pairs; i++)
	{
		auto send = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (last_hash)
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
					.link (nano::dev::genesis->account ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (last_hash))
					.build ();
		ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send).code);
		ASSERT_TRUE (store->block ().exists (*transaction, send->hash ()));
		auto receive = builder
					   .state ()
					   .account (nano::dev::genesis->account ())
					   .previous (send->hash ())
					   .representative (nano::dev::genesis->account ())
					   .balance (nano::dev::constants.genesis_amount)
					   .link (send->hash ())
					   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					   .work (*pool.generate (send->hash ()))
					   .build ();
		ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive).code);
		ASSERT_TRUE (store->block ().exists (*transaction, receive->hash ()));
		last_hash = receive->hash ();
	}
	ASSERT_EQ (0, store->pruned ().count (*transaction));
	ASSERT_EQ (send_receive_pairs * 2 + 1, store->block ().count (*transaction));
	// Pruning action
	ASSERT_EQ (send_receive_pairs * 2, ledger.pruning_action (*transaction, last_hash, 5));
	ASSERT_TRUE (store->pruned ().exists (*transaction, last_hash));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, last_hash));
	ASSERT_EQ (store->pruned ().count (*transaction), ledger.cache.pruned_count ());
	ASSERT_EQ (store->block ().count (*transaction), ledger.cache.block_count () - ledger.cache.pruned_count ());
	ASSERT_EQ (send_receive_pairs * 2, store->pruned ().count (*transaction));
	ASSERT_EQ (1, store->block ().count (*transaction)); // Genesis
}

TEST (ledger, pruning_source_rollback)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto epoch1 = builder
				  .state ()
				  .account (nano::dev::genesis->account ())
				  .previous (nano::dev::genesis->hash ())
				  .representative (nano::dev::genesis->account ())
				  .balance (nano::dev::constants.genesis_amount)
				  .link (ledger.epoch_link (nano::epoch::epoch_1))
				  .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				  .work (*pool.generate (nano::dev::genesis->hash ()))
				  .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *epoch1).code);
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (epoch1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (epoch1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Pruning action
	ASSERT_EQ (2, ledger.pruning_action (*transaction, send1->hash (), 1));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, epoch1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, epoch1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	nano::pending_info info;
	ASSERT_FALSE (store->pending ().get (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ()), info));
	ASSERT_EQ (nano::dev::genesis->account (), info.source);
	ASSERT_EQ (nano::Gxrb_ratio, info.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_1, info.epoch);
	// Receiving pruned block
	auto receive1 = builder
					.state ()
					.account (nano::dev::genesis->account ())
					.previous (send2->hash ())
					.representative (nano::dev::genesis->account ())
					.balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
					.link (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send2->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (2, ledger.cache.pruned_count ());
	ASSERT_EQ (5, ledger.cache.block_count ());
	// Rollback receive block
	ASSERT_FALSE (ledger.rollback (*transaction, receive1->hash ()));
	nano::pending_info info2;
	ASSERT_FALSE (store->pending ().get (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ()), info2));
	ASSERT_NE (nano::dev::genesis->account (), info2.source); // Tradeoff to not store pruned blocks accounts
	ASSERT_EQ (nano::Gxrb_ratio, info2.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_1, info2.epoch);
	// Process receive block again
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (2, ledger.cache.pruned_count ());
	ASSERT_EQ (5, ledger.cache.block_count ());
}

TEST (ledger, pruning_source_rollback_legacy)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	nano::keypair key1;
	auto send2 = builder
				 .send ()
				 .previous (send1->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - 2 * nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (key1.pub, send2->hash ())));
	auto send3 = builder
				 .send ()
				 .previous (send2->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - 3 * nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send2->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send3).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send3->hash ()));
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send3->hash ())));
	// Pruning action
	ASSERT_EQ (2, ledger.pruning_action (*transaction, send2->hash (), 1));
	ASSERT_FALSE (store->block ().exists (*transaction, send2->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send2->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	nano::pending_info info1;
	ASSERT_FALSE (store->pending ().get (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ()), info1));
	ASSERT_EQ (nano::dev::genesis->account (), info1.source);
	ASSERT_EQ (nano::Gxrb_ratio, info1.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_0, info1.epoch);
	nano::pending_info info2;
	ASSERT_FALSE (store->pending ().get (*transaction, nano::pending_key (key1.pub, send2->hash ()), info2));
	ASSERT_EQ (nano::dev::genesis->account (), info2.source);
	ASSERT_EQ (nano::Gxrb_ratio, info2.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_0, info2.epoch);
	// Receiving pruned block
	auto receive1 = builder
					.receive ()
					.previous (send3->hash ())
					.source (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send3->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (2, ledger.cache.pruned_count ());
	ASSERT_EQ (5, ledger.cache.block_count ());
	// Rollback receive block
	ASSERT_FALSE (ledger.rollback (*transaction, receive1->hash ()));
	nano::pending_info info3;
	ASSERT_FALSE (store->pending ().get (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ()), info3));
	ASSERT_NE (nano::dev::genesis->account (), info3.source); // Tradeoff to not store pruned blocks accounts
	ASSERT_EQ (nano::Gxrb_ratio, info3.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_0, info3.epoch);
	// Process receive block again
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	ASSERT_EQ (2, ledger.cache.pruned_count ());
	ASSERT_EQ (5, ledger.cache.block_count ());
	// Receiving pruned block (open)
	auto open1 = builder
				 .open ()
				 .source (send2->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (key1.pub)
				 .sign (key1.prv, key1.pub)
				 .work (*pool.generate (key1.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (key1.pub, send2->hash ())));
	ASSERT_EQ (2, ledger.cache.pruned_count ());
	ASSERT_EQ (6, ledger.cache.block_count ());
	// Rollback open block
	ASSERT_FALSE (ledger.rollback (*transaction, open1->hash ()));
	nano::pending_info info4;
	ASSERT_FALSE (store->pending ().get (*transaction, nano::pending_key (key1.pub, send2->hash ()), info4));
	ASSERT_NE (nano::dev::genesis->account (), info4.source); // Tradeoff to not store pruned blocks accounts
	ASSERT_EQ (nano::Gxrb_ratio, info4.amount.number ());
	ASSERT_EQ (nano::epoch::epoch_0, info4.epoch);
	// Process open block again
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	ASSERT_FALSE (store->pending ().exists (*transaction, nano::pending_key (key1.pub, send2->hash ())));
	ASSERT_EQ (2, ledger.cache.pruned_count ());
	ASSERT_EQ (6, ledger.cache.block_count ());
}

TEST (ledger, pruning_process_error)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_EQ (0, ledger.cache.pruned_count ());
	ASSERT_EQ (2, ledger.cache.block_count ());
	// Pruning action for latest block (not valid action)
	ASSERT_EQ (1, ledger.pruning_action (*transaction, send1->hash (), 1));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	// Attempt to process pruned block again
	ASSERT_EQ (nano::process_result::old, ledger.process (*transaction, *send1).code);
	// Attept to process new block after pruned
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::gap_previous, ledger.process (*transaction, *send2).code);
	ASSERT_EQ (1, ledger.cache.pruned_count ());
	ASSERT_EQ (2, ledger.cache.block_count ());
}

TEST (ledger, pruning_legacy_blocks)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	nano::keypair key1;
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .send ()
				 .previous (nano::dev::genesis->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store->pending ().exists (*transaction, nano::pending_key (nano::dev::genesis->account (), send1->hash ())));
	auto receive1 = builder
					.receive ()
					.previous (send1->hash ())
					.source (send1->hash ())
					.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
					.work (*pool.generate (send1->hash ()))
					.build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *receive1).code);
	auto change1 = builder
				   .change ()
				   .previous (receive1->hash ())
				   .representative (key1.pub)
				   .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				   .work (*pool.generate (receive1->hash ()))
				   .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *change1).code);
	auto send2 = builder
				 .send ()
				 .previous (change1->hash ())
				 .destination (key1.pub)
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (change1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	auto open1 = builder
				 .open ()
				 .source (send2->hash ())
				 .representative (nano::dev::genesis->account ())
				 .account (key1.pub)
				 .sign (key1.prv, key1.pub)
				 .work (*pool.generate (key1.pub))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *open1).code);
	auto send3 = builder
				 .send ()
				 .previous (open1->hash ())
				 .destination (nano::dev::genesis->account ())
				 .balance (0)
				 .sign (key1.prv, key1.pub)
				 .work (*pool.generate (open1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send3).code);
	// Pruning action
	ASSERT_EQ (3, ledger.pruning_action (*transaction, change1->hash (), 2));
	ASSERT_EQ (1, ledger.pruning_action (*transaction, open1->hash (), 1));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, receive1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, receive1->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, change1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, change1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	ASSERT_FALSE (store->block ().exists (*transaction, open1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, open1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, send3->hash ()));
	ASSERT_EQ (4, ledger.cache.pruned_count ());
	ASSERT_EQ (7, ledger.cache.block_count ());
	ASSERT_EQ (store->pruned ().count (*transaction), ledger.cache.pruned_count ());
	ASSERT_EQ (store->block ().count (*transaction), ledger.cache.block_count () - ledger.cache.pruned_count ());
}

TEST (ledger, pruning_safe_functions)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send1->hash ()));
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Pruning action
	ASSERT_EQ (1, ledger.pruning_action (*transaction, send1->hash (), 1));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (ledger.block_or_pruned_exists (*transaction, send1->hash ())); // true for pruned
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Safe ledger actions
	bool error (false);
	ASSERT_EQ (0, ledger.balance_safe (*transaction, send1->hash (), error));
	ASSERT_TRUE (error);
	error = false;
	ASSERT_EQ (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2, ledger.balance_safe (*transaction, send2->hash (), error));
	ASSERT_FALSE (error);
	error = false;
	ASSERT_EQ (0, ledger.amount_safe (*transaction, send2->hash (), error));
	ASSERT_TRUE (error);
	error = false;
	ASSERT_TRUE (ledger.account_safe (*transaction, send1->hash (), error).is_zero ());
	ASSERT_TRUE (error);
	error = false;
	ASSERT_EQ (nano::dev::genesis->account (), ledger.account_safe (*transaction, send2->hash (), error));
	ASSERT_FALSE (error);
}

TEST (ledger, hash_root_random)
{
	auto logger{ std::make_shared<nano::logger_mt> () };
	auto store = nano::make_store (logger, nano::unique_path (), nano::dev::constants);
	ASSERT_TRUE (!store->init_error ());
	nano::stat stats;
	nano::ledger ledger (*store, stats, nano::dev::constants);
	ledger.enable_pruning ();
	auto transaction (store->tx_begin_write ());
	store->initialize (*transaction, ledger.cache, ledger.constants);
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };
	nano::block_builder builder;
	auto send1 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (nano::dev::genesis->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (nano::dev::genesis->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send1).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send1->hash ()));
	auto send2 = builder
				 .state ()
				 .account (nano::dev::genesis->account ())
				 .previous (send1->hash ())
				 .representative (nano::dev::genesis->account ())
				 .balance (nano::dev::constants.genesis_amount - nano::Gxrb_ratio * 2)
				 .link (nano::dev::genesis->account ())
				 .sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				 .work (*pool.generate (send1->hash ()))
				 .build ();
	ASSERT_EQ (nano::process_result::progress, ledger.process (*transaction, *send2).code);
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Pruning action
	ASSERT_EQ (1, ledger.pruning_action (*transaction, send1->hash (), 1));
	ASSERT_FALSE (store->block ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->pruned ().exists (*transaction, send1->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, nano::dev::genesis->hash ()));
	ASSERT_TRUE (store->block ().exists (*transaction, send2->hash ()));
	// Test random block including pruned
	bool done (false);
	auto iteration (0);
	while (!done)
	{
		++iteration;
		auto root_hash (ledger.hash_root_random (*transaction));
		done = (root_hash.first == send1->hash ()) && (root_hash.second.is_zero ());
		ASSERT_LE (iteration, 1000);
	}
	done = false;
	while (!done)
	{
		++iteration;
		auto root_hash (ledger.hash_root_random (*transaction));
		done = (root_hash.first == send2->hash ()) && (root_hash.second == send2->root ().as_block_hash ());
		ASSERT_LE (iteration, 1000);
	}
}

TEST (ledger, unconfirmed_frontiers)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	nano::work_pool pool{ nano::dev::network_params.network, std::numeric_limits<unsigned>::max () };

	auto unconfirmed_frontiers = ledger.unconfirmed_frontiers ();
	ASSERT_TRUE (unconfirmed_frontiers.empty ());

	nano::state_block_builder builder;
	nano::keypair key;
	auto const latest = ledger.latest (*store.tx_begin_read (), nano::dev::genesis->account ());
	auto send = builder.make_block ()
				.account (nano::dev::genesis->account ())
				.previous (latest)
				.representative (nano::dev::genesis->account ())
				.balance (nano::dev::constants.genesis_amount - 100)
				.link (key.pub)
				.sign (nano::dev::genesis_key.prv, nano::dev::genesis_key.pub)
				.work (*pool.generate (latest))
				.build ();

	ASSERT_EQ (nano::process_result::progress, ledger.process (*store.tx_begin_write (), *send).code);

	unconfirmed_frontiers = ledger.unconfirmed_frontiers ();
	ASSERT_EQ (unconfirmed_frontiers.size (), 1);
	ASSERT_EQ (unconfirmed_frontiers.begin ()->first, 1);
	nano::uncemented_info uncemented_info1{ latest, send->hash (), nano::dev::genesis->account () };
	auto uncemented_info2 = unconfirmed_frontiers.begin ()->second;
	ASSERT_EQ (uncemented_info1.account, uncemented_info2.account);
	ASSERT_EQ (uncemented_info1.cemented_frontier, uncemented_info2.cemented_frontier);
	ASSERT_EQ (uncemented_info1.frontier, uncemented_info2.frontier);
}

TEST (ledger, is_send_genesis)
{
	auto ctx = nano::test::context::ledger_empty ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto tx = store.tx_begin_read ();
	ASSERT_FALSE (ledger.is_send (*tx, *nano::dev::genesis));
}

TEST (ledger, is_send_state)
{
	auto ctx = nano::test::context::ledger_send_receive ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto tx = store.tx_begin_read ();
	ASSERT_TRUE (ledger.is_send (*tx, *ctx.blocks ()[0]));
	ASSERT_FALSE (ledger.is_send (*tx, *ctx.blocks ()[1]));
}

TEST (ledger, is_send_legacy)
{
	auto ctx = nano::test::context::ledger_send_receive_legacy ();
	auto & ledger = ctx.ledger ();
	auto & store = ctx.store ();
	auto tx = store.tx_begin_read ();
	ASSERT_TRUE (ledger.is_send (*tx, *ctx.blocks ()[0]));
	ASSERT_FALSE (ledger.is_send (*tx, *ctx.blocks ()[1]));
}
