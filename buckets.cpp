#include"encodetotext.hpp"
#include<string>
#include<iostream>
#include<fstream>
#include<unordered_map>
#include<cassert>
#include<algorithm>
#include<cmath>
int main()
{
	std::unordered_map<small_string, unsigned short> h;
	std::ifstream wf("words.quickstart");
	small_string w;
	h.reserve(wf.good() << 16);
	unsigned short j = 0;
	while(wf >> w)
	{
		h[w] = j++;
	}
	assert(j == 0);
	decltype(h.bucket_count()) max_bucket_size = 0;
	double sum_sq = 0, sum = 0;
	for (decltype(h.bucket_count()) b = 0; b < h.bucket_count(); ++b)
	{
		const auto bsize = h.bucket_size(b);
		max_bucket_size = std::max(max_bucket_size, bsize);
		sum_sq += double(bsize) * double(bsize);
		sum += bsize;
		std::cout << '[' << b << "] " << bsize << ": ";
		for (auto it = h.cbegin(b); it != h.cend(b); ++it)
		{
			std::cout << it->first << '(' << it->second << ") ";
		}
		std::cout << '\n';
	}
	const double bucket_size_stddev = std::sqrt(sum_sq / h.bucket_count() - (sum / h.bucket_count()) * (sum / h.bucket_count()));
	std::cout << "\nload_factor: " << h.load_factor() << " ; max: " << h.max_load_factor();
	std::cout << "\nmax_bucket_size: " << max_bucket_size << " ; stddev: " << bucket_size_stddev << '\n';
}
