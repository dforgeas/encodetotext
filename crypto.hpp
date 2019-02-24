#include "btea.h"

namespace detail {

template <int N>
void Xor(uint32 (&dest)[N], uint32 const (&src)[N])
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

	CbcMac(uint32 const (&key)[4])
	{
		for (int i = 0; i < 4; i++)
		{
			k1[i] = k2[i] = key[i];
		}
		// Taken from HMAC:
		constexpr uint32 m1 = 0x36363636, m2 = 0x5c5c5c5c;
		constexpr uint32 ipad[4] = {m1, m1, m1, m1};
		constexpr uint32 opad[4] = {m2, m2, m2, m2};
		detail::Xor(k1, ipad);
		detail::Xor(k2, opad);
	}

	void update(uint32 const (&data)[stateSize])
	{
		detail::Xor(state, data);
		btea(state, stateSize, k1);
	}

	uint32 const (&finish())[stateSize]
	{
		btea(state, stateSize, k2);
		return state;
	}

private:
	uint32 k1[4];
	uint32 k2[4];
	uint32 state[stateSize] = {};
};
