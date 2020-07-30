#pragma once

#include <memory>

#include "DataReader.h"
#include "DataWriter.h"

enum class PacketType : uint8_t
{
	Welcome
};

// Basic serialization layer on top of ENet.
struct Packet
{
	const PacketType Type;

	Packet(PacketType type)
		: Type(type)
	{
	}

	virtual ~Packet() = default;

	virtual void Read(DataReader& reader) = 0;
	virtual void Write(DataWriter& writer) = 0;

	template<typename T>
	static std::shared_ptr<T> Create();

	static std::shared_ptr<Packet> CreateFromID(uint8_t type);
};

// The Welcome packet is the first packet sent, from the server to the client.
// It informs the client of it's internal ID.
struct WelcomePacket : public Packet
{
	uint32_t ClientID = 0;

	WelcomePacket()
		: Packet(PacketType::Welcome)
	{
	}

	void Read(DataReader& reader) override
	{
		ClientID = reader.Read<uint32_t>();
	}

	void Write(DataWriter& writer) override
	{
		writer.Write<uint32_t>(ClientID);
	}
};

template<typename T>
inline std::shared_ptr<T> Packet::Create() { return std::make_shared<T>(); }

inline std::shared_ptr<Packet> Packet::CreateFromID(uint8_t id)
{
	PacketType type = static_cast<PacketType>(id);

	switch (type)
	{
	case PacketType::Welcome: return std::make_shared<WelcomePacket>();
	default: assert(!"Unknown packet ID!");
	}
}
