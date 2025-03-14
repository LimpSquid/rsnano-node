#include <nano/lib/threading.hpp>
#include <nano/lib/timer.hpp>
#include <nano/secure/store.hpp>

auto nano::unchecked_store::equal_range (nano::transaction const & transaction, nano::block_hash const & dependency) -> std::pair<iterator, iterator>
{
	nano::unchecked_key begin_l{ dependency, 0 };
	nano::unchecked_key end_l{ nano::block_hash{ dependency.number () + 1 }, 0 };
	// Adjust for edge case where number () + 1 wraps around.
	auto end_iter = begin_l.previous < end_l.previous ? lower_bound (transaction, end_l) : end ();
	return std::make_pair (lower_bound (transaction, begin_l), std::move (end_iter));
}

auto nano::unchecked_store::full_range (nano::transaction const & transaction) -> std::pair<iterator, iterator>
{
	return std::make_pair (begin (transaction), end ());
}
