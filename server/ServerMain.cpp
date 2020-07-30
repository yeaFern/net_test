#include <iostream>
#include <enet.h>

static constexpr auto Port = 26456;
static constexpr auto MaxClients = 2;
static constexpr auto Timestep = 30;
static constexpr auto EnetWaitTime = 1000 / Timestep;

static ENetHost* s_Server;
static bool s_Running = true;

static void CreateServer()
{
	ENetAddress address = { 0 };
	address.host = ENET_HOST_ANY;
	address.port = Port;

	s_Server = enet_host_create(&address, MaxClients, 1, 0, 0);
	if (s_Server == nullptr)
	{
		std::cout << "Failed to create ENet host." << std::endl;
		std::exit(1);
	}
}

static void NetworkPoll()
{
	ENetEvent event;
	while (enet_host_service(s_Server, &event, EnetWaitTime) > 0)
	{
		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT: {
			std::cout << "ENET_EVENT_TYPE_CONNECT" << std::endl;
		} break;
		case ENET_EVENT_TYPE_DISCONNECT: {
			std::cout << "ENET_EVENT_TYPE_DISCONNECT" << std::endl;
		} break;
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
			std::cout << "ENET_EVENT_TYPE_DISCONNECT_TIMEOUT" << std::endl;
		} break;
		case ENET_EVENT_TYPE_RECEIVE: {
			std::cout << "ENET_EVENT_TYPE_RECEIVE" << std::endl;
		} break;
		}
	}
}

static void RunServer()
{
	CreateServer();

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
