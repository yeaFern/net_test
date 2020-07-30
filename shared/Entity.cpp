#include "Entity.h"

void Entity::Update(const InputSnapshot& input)
{
	this->X += input.DeltaX * Speed * input.DeltaTime;
	this->Y += input.DeltaY * Speed * input.DeltaTime;
}
