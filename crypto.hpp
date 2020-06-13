#pragma once
#include "btea.h"

#include <algorithm>

namespace detail {

template <int N>
void Xor(uint32_t (&dest)[N], uint32_t const (&src)[N])
{
	for (int i = 0; i < N; i++)
	{
		dest[i] ^= src[i];
	}
}

}

class CbcMac
{
public:
	static constexpr int stateSize = 5; // 5*32 = 160 bits, like SHA1

	CbcMac(uint32_t const (&key)[4])
	{
		for (int i = 0; i < 4; i++)
		{
			k1[i] = k2[i] = key[i];
		}
		// Taken from HMAC:
		constexpr uint32_t m1 = 0x36363636, m2 = 0x5c5c5c5c;
		constexpr uint32_t ipad[4] = {m1, m1, m1, m1};
		constexpr uint32_t opad[4] = {m2, m2, m2, m2};
		detail::Xor(k1, ipad);
		detail::Xor(k2, opad);
	}

	CbcMac(const CbcMac& copy) = default;

	void update(uint32_t const (&data)[stateSize])
	{
		detail::Xor(state, data);
		btea(state, stateSize, k1);
	}

	uint32_t const (& digest() const)[stateSize]
	{ // computes the digest with a copy of the state so that
	  // you may still call update next

		std::copy(state, state + stateSize, state2);
		btea(state2, stateSize, k2);
		return state2;
	}

private:
	uint32_t k1[4];
	uint32_t k2[4];
	uint32_t state[stateSize] = {};
	mutable uint32_t state2[stateSize];
};
