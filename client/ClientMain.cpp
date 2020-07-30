#include <olcPixelGameEngine.h>

#include "Entity.h"

class NetworkedGame : public olc::PixelGameEngine
{
private:
	Entity m_Player;
public:
	bool OnUserCreate() override
	{
		return true;
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

	bool OnUserUpdate(float dt) override
	{
		// Player input.
		if (auto input = GetPlayerInput(); input.HasInput())
		{
			// Apply the input locally right away (prediction).
			m_Player.Update(input, dt);
		}

		// Render.
		Clear(olc::BLACK);
		FillRect(m_Player.X, m_Player.Y, 16, 16);

		return true;
	}
};

int main(int argc, char** argv)
{
	NetworkedGame game;
	game.Construct(640, 360, 2, 2);
	game.Start();

	return 0;
}