#include "Entity.h"

void Entity::Update(const InputSnapshot& input, float dt)
{
	this->X += input.DeltaX * Speed * dt;
	this->Y += input.DeltaY * Speed * dt;
}
