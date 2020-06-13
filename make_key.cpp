#include "make_key.hpp"
#include "crypto.hpp"

#include <fstream>
#include <arpa/inet.h>

void make_key(const string_view password)
{
	constexpr uint32_t mac_key[] = {1, 2, 3, 4}; // not required to be secret
	CbcMac mac(mac_key);
	uint32_t buffer[CbcMac::stateSize] = {};
	for (auto it = password.begin() ; it != password.end(); )
	{
		// buffer from the previous iteration is kept even if password
		// hasn't got enough characters for a whole buffer: this is fine.
		// use a simple scheme: one uint32_t per character of the password
		for (int i = 0; i < CbcMac::stateSize and it != password.end(); ++i, ++it)
		{
			buffer[i] = static_cast<signed char>(*it); // the sign is extended so '\xab' gives 0xffffffab
		}
		mac.update(buffer);
	}

	const auto &key = mac.digest();
	std::ofstream out("encode.key", std::ios::binary);
	for (int i = 0; i < 4; ++i)
	{
		const uint32_t x = htonl(key[i]);
		out.write(reinterpret_cast<const char*>(&x), sizeof x);
	}
}
