#pragma once

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>

struct Transform
{
	glm::vec3 position;
	glm::vec3 euler;
	glm::quat rotation;
	glm::vec3 scale;
	glm::mat4 model;

	Transform()
	{
		position = glm::vec3(0.0f);
		rotation = glm::quat(glm::vec3(glm::radians(0.0f)));
		euler = glm::vec3(0.0f);
		scale = glm::vec3(1.0f);
		model = glm::mat4(1.0f);
	}

	inline glm::vec3 forward()
	{
		return rotation * glm::vec3(0.0f, 0.0f, 1.0f);
	}

	inline glm::vec3 up()
	{
		return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
	}

	inline glm::vec3 left()
	{
		return rotation * glm::vec3(1.0f, 0.0f, 0.0f);
	}

	inline void rotate_with_euler(glm::vec3 e)
	{
		glm::quat pitch = glm::quat(glm::vec3(glm::radians(e.x), glm::radians(0.0f), glm::radians(0.0f)));
		glm::quat yaw = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(e.y), glm::radians(0.0f)));
		glm::quat roll = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(e.z)));

		rotation = yaw * pitch * roll;
	}

	inline void update()
	{
		glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
		glm::mat4 r = glm::mat4_cast(rotation);
		glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

		model = t * r * s;
	}
};