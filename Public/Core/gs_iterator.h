// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspacePlatform.h"

namespace GS
{

/**
 * modulo_iteration iterates over values in the range 0...N-1 (inclusive)
 * in a shuffled manner. This can be useful in situations where a statistically
 * randomized order is not necessary.
 *
 * Starting at any index in the valid range, and given a prime number P larger
 * than N, repeatedly computing cur_index = (cur_index + P) % N will produce
 * every value in the range before repeating.
 *
 * Usage options:
 *
 * GS::modulo_iteration iter(N);
 *
 * int idx = 0;
 * while (iter.next_index(idx)) { .. }
 *
 * while (!iter.done()) {
 *   int idx = iter.get_and_increment();
 *   ...
 * }
 *
 * for (int idx = iter.get(); iter.increment(); idx = iter.get()) { .. }
 *
 */
struct modulo_iteration
{
	static constexpr uint64_t modulo_prime = 4294967311ull; // this is a prime number larger than max 32-bit unsigned int

	uint64_t N = 0;			// number of values to iterate over (ie iteration is i < N)
	uint64_t cur_index = 0;	// current modulo-index
	uint64_t i = 0;			// current linear index
	uint64_t modN = 1;		// we modulo by modN rather than N to handle N == 0 case   ( int % 0 is a divide-by-zero )

	modulo_iteration(uint32_t Num)
	{
		N = (uint64_t)std::max((uint32_t)0, Num);
		modN = std::max(1ull, N);
		cur_index = ( (N/2) + modulo_prime ) % modN;	// do one initial mod-iteration, from middle of range
		i = 0;
	}

	modulo_iteration(int32_t Num)
	{
		N = (uint64_t)std::max((int32_t)0, Num);
		modN = std::max(1ull, N);
		cur_index = ( (N/2) + modulo_prime ) % modN;	// do one initial mod-iteration, from middle of range
		i = 0;
	}

	// get current index, increment, and return false if iteration is complete
	bool next_index(uint32_t& index)
	{
		index = (uint32_t)cur_index;
		cur_index = (cur_index + modulo_prime) % modN;
		return (i++ != N);
	}

	// get current index, increment, and return false if iteration is complete
	bool next_index(int32_t& index)
	{
		index = (int32_t)cur_index;
		cur_index = (cur_index + modulo_prime) % modN;
		return (i++ != N);
	}

	// get the current index
	int32_t get() const {
		return (int32_t)cur_index;
	}

	// get the current linear index
	int32_t get_linear_index() const {
		return (int32_t)i;
	}

	//! get current index and then increments
	int32_t get_and_increment() {
		int32_t index = (int32_t)cur_index;
		cur_index = (cur_index + modulo_prime) % modN;
		i++;
		return index;
	}

	//! increments, and then returns false if iteration is complete. Use get() on return true
	bool increment() {
		cur_index = (cur_index + modulo_prime) % modN;
		return (i++ != N);
	}

	//! returns true if iteration is complete
	bool done() const {
		return i >= N;
	}
};


}
