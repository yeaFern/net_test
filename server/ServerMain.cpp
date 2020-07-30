#include <iostream>
#include <array>
#include <chrono>
#include <cassert>
#include <enet.h>

#include "SharedConfig.h"
#include "Packet.h"
#include "Entity.h"

static constexpr auto ENetWaitTime = 1000 / Config::ServerTimestep;

static uint64_t s_ServerStartTime;
static ENetHost* s_Server;
static bool s_Running = true;

struct Client
{
	Entity Entity;
	uint32_t LastInput;
};

static std::array<Client*, Config::MaxClients> s_Clients;
static int s_ClientCount = 0;

uint64_t GetMilliseconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t GetTime()
{
	return GetMilliseconds() - s_ServerStartTime;
}

static void CreateServer()
{
	ENetAddress address = { 0 };
	address.host = ENET_HOST_ANY;
	address.port = Config::Port;

	s_Server = enet_host_create(&address, Config::MaxClients, 1, 0, 0);
	if (s_Server == nullptr)
	{
		std::cout << "Failed to create ENet host." << std::endl;
		std::exit(1);
	}
}

// Finds a free client ID in the global client pool and returns it.
static uint32_t AssignClient()
{
	for (uint32_t i = 0; i < Config::MaxClients; i++)
	{
		if (s_Clients[i] == nullptr)
		{
			s_Clients[i] = new Client;
			return i;
		}
	}

	// In theory we should never reach this point, as ENet will not accept more connections than
	// we specified with MaxClients.
	assert(!"Failed to assign client ID!");
}

// Frees a client ID from the global pool.
static void UnassignClient(uint32_t id)
{
	if (s_Clients[id] != nullptr)
	{
		delete s_Clients[id];
		s_Clients[id] = nullptr;
	}
}

// Sends a packet to a specific client.
static void SendPacket(ENetPeer* peer, const std::shared_ptr<Packet>& packet)
{
	// Write the packet to a buffer.
	DataWriter writer;
	writer.Write<uint8_t>(static_cast<uint8_t>(packet->Type));
	packet->Write(writer);

	// Hand it off to ENet.
	// Note that ENet will copy the data to its own internal buffer.
	enet_peer_send(peer, 0, enet_packet_create(writer.GetData(), writer.GetSize(), ENET_PACKET_FLAG_RELIABLE));
}

// Sends a packet to all clients which are connected.
static void BroadcastPacket(const std::shared_ptr<Packet>& packet)
{
	// Write the packet to a buffer.
	DataWriter writer;
	writer.Write<uint8_t>(static_cast<uint8_t>(packet->Type));
	packet->Write(writer);

	// Hand it off to ENet.
	// Note that ENet will copy the data to its own internal buffer.
	enet_host_broadcast(s_Server, 0, enet_packet_create(writer.GetData(), writer.GetSize(), ENET_PACKET_FLAG_RELIABLE));
}

// Use this to check for cheating.
// TODO: Implement a better check. Should probably check how far this movement will move the player and discard
//       based on that.
static bool ValidateInput(const InputSnapshot& input)
{
	if (input.DeltaX > 1.0f || input.DeltaY > 1.0f || input.DeltaTime > 1.0f)
	{
		return false;
	}
	else
	{
		return true;
	}
}

static void HandlePacket(const std::shared_ptr<Packet>& p, uint32_t clientID)
{
	auto client = s_Clients[clientID];
	if (client == nullptr) { return; }

	switch (p->Type)
	{
	case PacketType::Input: {
		auto packet = std::dynamic_pointer_cast<InputPacket>(p);
		if (ValidateInput(packet->Input))
		{
			client->Entity.Update(packet->Input);
			client->LastInput = packet->Input.SequenceNumber;
		}
	} break;
	}
}

static void NetworkPoll()
{
	// The timeout value given to enet_host_service is the amount of time to wait until an event is received.
	// This means that enet_host_service will have to go the entire timeout without receiving a single event in order to return.
	// In a multiplayer scenario this is somewhat unlikely and will result in this call hanging forever, and the world will not
	// be ticked at all.
	// To fix this, we implement our own timer which will keep track of the time to poll for network events, and pass a timeout
	// of zero to enet_host_service.
	uint64_t start = GetTime();
	while (GetTime() - start < ENetWaitTime)
	{
		ENetEvent event;
		if (enet_host_service(s_Server, &event, 0) > 0)
		{
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT: {
				// When a new client connects we will assign them an ID and send them a welcome packet
				// containing their ID.
				uint32_t id = AssignClient();
				event.peer->data = reinterpret_cast<void*>(id);
				s_ClientCount++;
				std::cout << "Client connected, " << s_ClientCount << "/" << Config::MaxClients << "." << std::endl;

				// Create a new packet to send to the client.
				auto packet = Packet::Create<WelcomePacket>();
				packet->ClientID = id;
				SendPacket(event.peer, packet);
			} break;
			case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: case ENET_EVENT_TYPE_DISCONNECT: {
				// When a client disconnects or times out, we can free their ID from the global pool.
				uint32_t id = reinterpret_cast<uint32_t>(event.peer->data);
				UnassignClient(id);
				s_ClientCount--;
				std::cout << "Client disconnected, " << s_ClientCount << "/" << Config::MaxClients << "." << std::endl;
			} break;
			case ENET_EVENT_TYPE_RECEIVE: {
				uint32_t id = reinterpret_cast<uint32_t>(event.peer->data);

				DataReader reader(event.packet->data, event.packet->dataLength);

				// Read the packet ID from the buffer.
				uint8_t packetID = reader.Read<uint8_t>();

				// Create the packet based on its ID.
				auto packet = Packet::CreateFromID(packetID);

				// Read the rest of the packet from the buffer.
				packet->Read(reader);

				// Handle the packet.
				HandlePacket(packet, id);

				enet_packet_destroy(event.packet);
			} break;
			}
		}
	}
}

static void RunServer()
{
	CreateServer();

	std::cout << "Server listening on port " << Config::Port << "." << std::endl;
	while (s_Running)
	{
		// Poll for incoming packets.
		NetworkPoll();

		// Send new world state out to clients.
		auto packet = Packet::Create<WorldStatePacket>();
		for (uint32_t i = 0; i < Config::MaxClients; i++)
		{
			auto client = s_Clients[i];
			if (client == nullptr) { continue; }
			WorldStatePacket::Entry entry;
			entry.EntityID = i;
			entry.PreviousInput = client->LastInput;
			entry.X = client->Entity.X;
			entry.Y = client->Entity.Y;
			packet->Entries.push_back(entry);
		}
		BroadcastPacket(packet);
	}
}

int main(int argc, char** argv)
{
	s_ServerStartTime = GetMilliseconds();

	if (enet_initialize() != 0)
	{
		std::cout << "Failed to initialize ENet." << std::endl;
		std::exit(1);
	}

	RunServer();

	enet_deinitialize();
	return 0;
}
