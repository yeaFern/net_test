#pragma once

#include <cstdint>
#include <cassert>

// Used to read data from a buffer.
class DataReader
{
private:
	uint8_t* m_Buffer;
	size_t m_Size;
	size_t m_Ptr;
public:
	DataReader(uint8_t* buffer, size_t size)
		: m_Buffer(buffer), m_Size(size), m_Ptr(0)
	{
	}

	template<typename T>
	T Read()
	{
		assert(m_Ptr + sizeof(T) <= m_Size);
		T result;
		result = *((T*)(m_Buffer + m_Ptr));
		m_Ptr += sizeof(T);
		return result;
	}
};
