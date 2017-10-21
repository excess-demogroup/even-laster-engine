#include <vector>
#include <algorithm>
#include <iterator>

class BlobBuilder
{
public:
	template <typename T>
	void append(const T &data)
	{
		std::copy(reinterpret_cast<const uint8_t*>(&data),
			reinterpret_cast<const uint8_t*>(&data + 1),
			std::back_inserter(bytes));
	}

	std::vector<uint8_t> getBytes() { return bytes; }

private:
	std::vector<uint8_t> bytes;
};
