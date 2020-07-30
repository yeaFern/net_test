#include <iostream>
#include <array>
#include <cassert>
#include <enet.h>

#include "SharedConfig.h"
#include "Packet.h"
#include "Entity.h"

static constexpr auto MaxClients = 32;
static constexpr auto EnetWaitTime = 1000 / Config::ServerTimestep;

static ENetHost* s_Server;
static bool s_Running = true;
static std::array<Entity*, MaxClients> s_Clients;
static int s_ClientCount = 0;

static void CreateServer()
{
	ENetAddress address = { 0 };
	address.host = ENET_HOST_ANY;
	address.port = Config::Port;

	s_Server = enet_host_create(&address, MaxClients, 1, 0, 0);
	if (s_Server == nullptr)
	{
		std::cout << "Failed to create ENet host." << std::endl;
		std::exit(1);
	}
}

// Finds a free client ID in the global client pool and returns it.
static uint32_t AssignClient()
{
	for (uint32_t i = 0; i < MaxClients; i++)
	{
		if (s_Clients[i] == nullptr)
		{
			s_Clients[i] = new Entity;
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

static void NetworkPoll()
{
	ENetEvent event;
	while (enet_host_service(s_Server, &event, EnetWaitTime) > 0)
	{
		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT: {
			// When a new client connects we will assign them an ID and send them a welcome packet
			// containing their ID.
			uint32_t id = AssignClient();
			event.peer->data = reinterpret_cast<void*>(id);
			s_ClientCount++;
			std::cout << "Client connected, " << s_ClientCount << "/" << MaxClients << "." << std::endl;

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
			std::cout << "Client disconnected, " << s_ClientCount << "/" << MaxClients << "." << std::endl;
		} break;
		case ENET_EVENT_TYPE_RECEIVE: {
			std::cout << "ENET_EVENT_TYPE_RECEIVE" << std::endl;
			enet_packet_destroy(event.packet);
		} break;
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
	}
}

int main(int argc, char** argv)
{
	if (enet_initialize() != 0)
	{
		std::cout << "Failed to initialize ENet." << std::endl;
		std::exit(1);
	}

	RunServer();

	enet_deinitialize();
	return 0;
}
