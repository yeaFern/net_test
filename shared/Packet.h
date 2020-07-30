#pragma once

#include <memory>

#include "Entity.h"

#include "DataReader.h"
#include "DataWriter.h"

enum class PacketType : uint8_t
{
	Welcome,
	Input,
	WorldState
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

// The Input packet is sent whenever the user supplies some input.
// It is used by the server to calculate the position of the player.
struct InputPacket : public Packet
{
	InputSnapshot Input;
	float DeltaTime = 0.0f;

	InputPacket()
		: Packet(PacketType::Input)
	{
	}

	void Read(DataReader& reader) override
	{
		Input.DeltaX = reader.Read<float>();
		Input.DeltaY = reader.Read<float>();
		DeltaTime = reader.Read<float>();
	}

	void Write(DataWriter& writer) override
	{
		writer.Write<float>(Input.DeltaX);
		writer.Write<float>(Input.DeltaY);
		writer.Write<float>(DeltaTime);
	}
};

// The World State packet is sent to all the clients to inform them of the
// new positions of the entities in the world.
struct WorldStatePacket : public Packet
{
	struct Entry
	{
		uint32_t EntityID;
		float X;
		float Y;
	};

	std::vector<Entry> Entries;

	WorldStatePacket()
		: Packet(PacketType::WorldState)
	{
	}

	void Read(DataReader& reader) override
	{
		uint32_t count = reader.Read<uint32_t>();
		for (int i = 0; i < count; i++)
		{
			WorldStatePacket::Entry entry;
			entry.EntityID = reader.Read<uint32_t>();
			entry.X = reader.Read<float>();
			entry.Y = reader.Read<float>();
			Entries.push_back(entry);
		}
	}

	void Write(DataWriter& writer) override
	{
		writer.Write<uint32_t>(Entries.size());
		for (const auto& entry : Entries)
		{
			writer.Write<uint32_t>(entry.EntityID);
			writer.Write<float>(entry.X);
			writer.Write<float>(entry.Y);
		}
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
	case PacketType::Input: return std::make_shared<InputPacket>();
	case PacketType::WorldState: return std::make_shared<WorldStatePacket>();
	default: assert(!"Unknown packet ID!");
	}
}
