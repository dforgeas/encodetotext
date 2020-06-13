#include <iostream>

#include "btea.h"
#define LENGTH(array) (sizeof(array) / sizeof *(array))

template <typename T, typename T2>
std::ostream& printArray(std::ostream& out, const T array[], const size_t length)
{
	out << '[';
	for (size_t i = 0; i < length; ++i)
	{
		if (i != 0) out << ", ";
		out << static_cast<T2>(array[i]);
	}
	return out << ']';
}

int main()
{
	uint32_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
	uint32_t key[] = {123, 456, 789, 012};

	printArray<uint32_t, int>(
		printArray<uint32_t, int>(std::cout << "data: ", data, LENGTH(data))
			<< "\nkey: ", key, LENGTH(key)
	) << '\n';
	btea(data, LENGTH(data), key);
	printArray<uint32_t, int>(std::cout << "crypted: ", data, LENGTH(data)) << '\n';

	btea(data, -LENGTH(data), key);
	printArray<uint32_t, int>(std::cout << "clear: ", data, LENGTH(data)) << '\n';
}
