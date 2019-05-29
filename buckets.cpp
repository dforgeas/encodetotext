#include<string>
#include<iostream>
#include<fstream>
#include<unordered_map>
#include<cassert>
int main()
{
	std::unordered_map<std::string, unsigned short> h;
	std::ifstream wf("words.quickstart");
	std::string w;
	h.reserve(wf.good() << 16);
	unsigned short j = 0;
	while(std::getline(wf, w))
	{
		h[w] = j++;
	}
	assert(j == 0);
	for (decltype(h.bucket_count()) b = 0; b < h.bucket_count(); ++b)
	{
		std::cout << '[' << b << "] " << h.bucket_size(b) << ": ";
		for (auto it = h.cbegin(b); it != h.cend(b); ++it)
		{
			std::cout << it->first << '(' << it->second << ") ";
		}
		std::cout << '\n';
	}
	std::cout << "\nload_factor: " << h.load_factor() << " ; max: " << h.max_load_factor() << '\n';
}
