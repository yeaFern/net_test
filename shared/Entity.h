#pragma once

// Represents a snapshot of an entities input at some particular point in time.
struct InputSnapshot
{
	float DeltaX = 0;
	float DeltaY = 0;

	InputSnapshot() = default;
	InputSnapshot(const InputSnapshot&) = default;

	InputSnapshot(float dx, float dy)
		: DeltaX(dx), DeltaY(dy)
	{
	}

	// Returns true if the struct contains any useful information, false otherwise.
	bool HasInput() const
	{
		return DeltaX != 0 || DeltaY != 0;
	}
};

// Represents an entity in the world.
class Entity
{
public:
	static constexpr float Speed = 128.0f;
public:
	float X = 0;
	float Y = 0;
public:
	// Updates this entities position based on the supplied input and delta time.
	void Update(const InputSnapshot& input, float dt);
};
