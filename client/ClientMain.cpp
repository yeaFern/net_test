#include <array>
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

struct EntityPosition
{
	float Timestamp = 0;
	float X = 0;
	float Y = 0;

	EntityPosition() = default;
	EntityPosition(const EntityPosition&) = default;

	EntityPosition(float ts, float x, float y)
		: Timestamp(ts), X(x), Y(y)
	{
	}
};

struct Player
{
	Entity WorldEntity;
	std::vector<EntityPosition> PositionBuffer;
};

class NetworkedGame : public olc::PixelGameEngine
{
private:
	ENetHost* m_Client;
	ENetPeer* m_Peer;
	bool m_Connected = false;
	GameState m_State = GameState::Handshaking;

	uint32_t m_PlayerID = -1;
	std::array<Player*, Config::MaxClients> m_Entities{ nullptr };

	float m_GameTime = 0.0f;

	std::vector<InputSnapshot> m_PendingInputs;
	uint32_t m_InputSequenceNumber = 0;
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

	InputSnapshot GetPlayerInput(float dt)
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

			return InputSnapshot(m_InputSequenceNumber++, dt, dx, dy);
		}

		// If the player didn't move, return an empty snapshot.
		return InputSnapshot();
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

	void SendPacket(const std::shared_ptr<Packet>& packet)
	{
		if (!m_Connected) { return; }

		// Write the packet to a buffer.
		DataWriter writer;
		writer.Write<uint8_t>(static_cast<uint8_t>(packet->Type));
		packet->Write(writer);

		// Hand it off to ENet.
		// Note that ENet will copy the data to its own internal buffer.
		enet_peer_send(m_Peer, 0, enet_packet_create(writer.GetData(), writer.GetSize(), ENET_PACKET_FLAG_RELIABLE));
	}

	void HandlePacket(const std::shared_ptr<Packet>& p)
	{
		switch (p->Type)
		{
		case PacketType::Welcome: {
			auto packet = std::dynamic_pointer_cast<WelcomePacket>(p);

			// Assign the players ID.
			m_PlayerID = packet->ClientID;

			// Create the players entity.
			m_Entities[m_PlayerID] = new Player;

			// Move to the playing state.
			m_State = GameState::Playing;
		} break;
		case PacketType::WorldState: {
			auto packet = std::dynamic_pointer_cast<WorldStatePacket>(p);

			for (auto& entry : packet->Entries)
			{
				if (entry.EntityID == m_PlayerID)
				{
					m_Entities[entry.EntityID]->WorldEntity.X = entry.X;
					m_Entities[entry.EntityID]->WorldEntity.Y = entry.Y;

					// Perform reconciliation.
					uint32_t j = 0;
					while (j < m_PendingInputs.size())
					{
						auto& input = m_PendingInputs[j];
						if (input.SequenceNumber <= entry.PreviousInput)
						{
							// This input has been processed by the server, so drop it.
							m_PendingInputs.erase(m_PendingInputs.begin() + j);
						}
						else
						{
							// This input has not been processed by the server yet, so reapply it.
							m_Entities[entry.EntityID]->WorldEntity.Update(input);
							j++;
						}
					}
				}
				else
				{
					if (m_Entities[entry.EntityID] == nullptr)
					{
						// If we encounter a new entity, create it.
						m_Entities[entry.EntityID] = new Player;
					}

					// Here we will do interpolation, but for now just set the entities position.
					// m_Entities[entry.EntityID]->WorldEntity.X = entry.X;
					// m_Entities[entry.EntityID]->WorldEntity.Y = entry.Y;

					m_Entities[entry.EntityID]->PositionBuffer.push_back(EntityPosition(m_GameTime, entry.X, entry.Y));
				}
			}
		} break;
		}
	}

	void InterpolateEntities()
	{
		// Some time in the past.
		float renderTimestamp = m_GameTime - (1.0f / Config::ServerTimestep);

		for (auto entity : m_Entities)
		{
			if (entity == nullptr) { continue; }
			if (entity == m_Entities[m_PlayerID]) { continue; }

			auto& buffer = entity->PositionBuffer;

			// Drop all older positions.
			while (buffer.size() >= 2 && buffer[1].Timestamp <= renderTimestamp)
			{
				buffer.erase(buffer.begin());
			}

			// Interpolate between the two newest positions.
			if (buffer.size() >= 2 && buffer[0].Timestamp <= renderTimestamp && renderTimestamp <= buffer[1].Timestamp)
			{
				auto x0 = buffer[0].X;
				auto x1 = buffer[1].X;
				auto y0 = buffer[0].Y;
				auto y1 = buffer[1].Y;

				auto t0 = buffer[0].Timestamp;
				auto t1 = buffer[1].Timestamp;

				entity->WorldEntity.X = x0 + (x1 - x0) * (renderTimestamp - t0) / (t1 - t0);
				entity->WorldEntity.Y = y0 + (y1 - y0) * (renderTimestamp - t0) / (t1 - t0);
			}
		}
	}

	bool OnUserUpdate(float dt) override
	{
		m_GameTime += dt;

		// Poll for incoming packets.
		NetworkPoll();

		// Utility input.
		if (GetKey(olc::Key::C).bPressed) { Connect(); }
		if (GetKey(olc::Key::ESCAPE).bPressed) { Disconnect(); }

		// Player input.
		if (m_State == GameState::Playing)
		{
			// Only allow player input in the playing state.
			if (auto input = GetPlayerInput(dt); input.HasInput())
			{
				// Send the input to the server.
				auto packet = Packet::Create<InputPacket>();
				packet->Input = input;
				SendPacket(packet);

				// Apply the input locally right away (prediction).
				m_Entities[m_PlayerID]->WorldEntity.Update(input);

				// Save the input for reconciliation.
				m_PendingInputs.push_back(input);
			}
		}

		InterpolateEntities();

		// Render.
		Clear(olc::BLACK);
		for (auto entity : m_Entities)
		{
			if (entity == nullptr) { continue; }
			FillRect(entity->WorldEntity.X, entity->WorldEntity.Y, 16, 16);
		}

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