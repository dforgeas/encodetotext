#include"encodetotext.hpp"
#include<vector>
#include<iostream>
#include<fstream>
#include<unordered_map>
#include<cassert>
#include<algorithm>
#include<cmath>
int main()
{
	std::ios::sync_with_stdio(false);
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
	std::vector<decltype(h.bucket_size(0))> bucket_sizes;
	bucket_sizes.reserve(h.bucket_count());
	double sum_sq = 0;
	for (decltype(h.bucket_count()) b = 0; b < h.bucket_count(); ++b)
	{
		const auto bsize = h.bucket_size(b);
		sum_sq += double(bsize) * double(bsize);
		bucket_sizes.push_back(bsize);
		std::clog << '[' << b << "] " << bsize << ": ";
		for (auto it = h.cbegin(b); it != h.cend(b); ++it)
		{
			std::clog << it->first << '(' << it->second << ") ";
		}
		std::clog << '\n';
	}
	std::clog << std::flush; // this wouldn't be needed with std::cerr remarquably

	const double bucket_size_stddev = std::sqrt(sum_sq / h.bucket_count() -
		h.load_factor() * h.load_factor());
	std::cout << "\nload_factor: " << h.load_factor()
		<< " ; max: " << h.max_load_factor() << std::endl;
	std::sort(bucket_sizes.begin(), bucket_sizes.end());
	std::cout << "max_bucket_size: " << bucket_sizes.back()
		<< " ; median: " << bucket_sizes[bucket_sizes.size() / 2] // h.bucket_count() is most probably odd
		<< " ; stddev: " << bucket_size_stddev << '\n';
}
