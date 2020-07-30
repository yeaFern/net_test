#include <olcPixelGameEngine.h>
#include <enet.h>

#include "SharedConfig.h"
#include "Packet.h"
#include "Entity.h"

static constexpr auto ConnectionTimeout = 800;
static constexpr auto DisconnectTimeout = 800;
static constexpr auto ServerAddress = "127.0.0.1";

// Represents the state of the game.
// The game is initially in the handshaking state, it switches to the playing state
// once the client has received it's ID from the server.
enum class GameState
{
	Handshaking,
	Playing
};

class NetworkedGame : public olc::PixelGameEngine
{
private:
	ENetHost* m_Client;
	ENetPeer* m_Peer;
	bool m_Connected = false;
	GameState m_State = GameState::Handshaking;

	uint32_t m_PlayerID = -1;

	Entity m_Player;
public:
	bool OnUserCreate() override
	{
		Connect();

		return true;
	}

	bool OnUserDestroy() override
	{
		Disconnect();

		return true;
	}

	void Connect()
	{
		if (m_Connected) { return; }

		m_Client = enet_host_create(nullptr, 1, 1, 0, 0);
		if (m_Client == nullptr)
		{
			std::cout << "Failed to create ENet host." << std::endl;
			return;
		}

		std::cout << "Attempting to connect to " << ServerAddress << ":" << Config::Port << "." << std::endl;

		ENetAddress address = { 0 };
		enet_address_set_host(&address, ServerAddress);
		address.port = Config::Port;

		m_Peer = enet_host_connect(m_Client, &address, 1, 0);
		if (m_Peer == nullptr)
		{
			std::cout << "Failed to initiate connection to peer." << std::endl;
			return;
		}

		ENetEvent event;
		if (enet_host_service(m_Client, &event, ConnectionTimeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		{
			std::cout << "Connected to server." << std::endl;
			m_Connected = true;
			return;
		}
		else
		{
			std::cout << "Failed to connect to server." << std::endl;
			enet_peer_reset(m_Peer);
			return;
		}
	}

	void Disconnect()
	{
		if (!m_Connected) { return; }

		ENetEvent event;
		enet_peer_disconnect(m_Peer, 0);
		while (enet_host_service(m_Client, &event, DisconnectTimeout) > 0)
		{
			switch (event.type)
			{
			case ENET_EVENT_TYPE_RECEIVE: {
				enet_packet_destroy(event.packet);
			} break;
			case ENET_EVENT_TYPE_DISCONNECT: {
				std::cout << "Gracefully disconnect from server." << std::endl;
				m_Connected = false;
				enet_host_destroy(m_Client);
				return;
			} break;
			}
		}

		enet_peer_reset(m_Peer);
		enet_host_destroy(m_Client);
		std::cout << "Forcefully disconnect from server." << std::endl;
		m_Connected = false;
	}

	InputSnapshot GetPlayerInput()
	{
		float dx = 0.0f;
		float dy = 0.0f;

		if (GetKey(olc::W).bHeld) { dy -= 1.0f; }
		if (GetKey(olc::A).bHeld) { dx -= 1.0f; }
		if (GetKey(olc::S).bHeld) { dy += 1.0f; }
		if (GetKey(olc::D).bHeld) { dx += 1.0f; }

		// Normalize the input if there was any.
		if (float l = std::sqrt(dx * dx + dy * dy) != 0)
		{
			dx /= l;
			dy /= l;

			return InputSnapshot(dx, dy);
		}

		// If the player didn't move, return an empty snapshot.
		return InputSnapshot(0.0f, 0.0f);
	}

	void NetworkPoll()
	{
		if (!m_Connected) { return; }

		ENetEvent event;
		while (enet_host_service(m_Client, &event, 1) > 0)
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
				DataReader reader(event.packet->data, event.packet->dataLength);

				// Read the packet ID from the buffer.
				uint8_t packetID = reader.Read<uint8_t>();

				// Create the packet based on its ID.
				auto packet = Packet::CreateFromID(packetID);

				// Read the rest of the packet from the buffer.
				packet->Read(reader);

				// Handle the packet.
				HandlePacket(packet);

				enet_packet_destroy(event.packet);
			} break;
			}
		}
	}

	void HandlePacket(const std::shared_ptr<Packet>& p)
	{
		switch (p->Type)
		{
		case PacketType::Welcome: {
			auto packet = std::dynamic_pointer_cast<WelcomePacket>(p);
			m_PlayerID = packet->ClientID;
			m_State = GameState::Playing;
		} break;
		}
	}

	bool OnUserUpdate(float dt) override
	{
		// Poll for incoming packets.
		NetworkPoll();

		// Utility input.
		if (GetKey(olc::Key::C).bPressed) { Connect(); }
		if (GetKey(olc::Key::ESCAPE).bPressed) { Disconnect(); }

		// Player input.
		if (m_State == GameState::Playing)
		{
			// Only allow player input in the playing state.
			if (auto input = GetPlayerInput(); input.HasInput())
			{
				// Apply the input locally right away (prediction).
				m_Player.Update(input, dt);
			}
		}

		// Render.
		Clear(olc::BLACK);
		FillRect(m_Player.X, m_Player.Y, 16, 16);

		if (m_Connected)
		{
			DrawString(2, 2, "Connected.", olc::DARK_GREEN);
		}
		else
		{
			DrawString(2, 2, "Not connected.", olc::DARK_RED);
		}

		return true;
	}
};

int main(int argc, char** argv)
{
	if (enet_initialize() != 0)
	{
		std::cout << "Failed to initialize ENet." << std::endl;
		std::exit(1);
	}

	NetworkedGame game;
	game.Construct(640, 360, 2, 2);
	game.Start();

	enet_deinitialize();
	return 0;
}