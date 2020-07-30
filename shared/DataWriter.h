#pragma once

#include <vector>
#include <cstdint>
#include <cassert>

// Used to write data to a buffer.
class DataWriter
{
private:
	std::vector<uint8_t> m_Buffer;
public:
	template<typename T>
	void Write(T value)
	{
		uint8_t* buf = (uint8_t*)&value;
		for (int i = 0; i < sizeof(T); i++)
		{
			m_Buffer.push_back(buf[i]);
		}
	}

	inline uint8_t* GetData() { return m_Buffer.data(); }
	inline size_t GetSize() { return m_Buffer.size(); }
};
