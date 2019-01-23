#pragma once

#include "transform.h"

enum ProjectionType : uint32_t
{
	PROJ_PERSPECTIVE,
	PROJ_OTHOGRAPHIC
};

struct PerspectiveInfo
{
	float aspect_ratio;
	float fov;
	float n;
	float f;
};

struct OrthographicInfo
{
	float l;
	float r;
	float t;
	float b;
	float n;
	float f;
};

struct ProjectionInfo
{
	ProjectionType type;
	union
	{
		OrthographicInfo othographic;
		PerspectiveInfo perspective;
	};
};

struct Camera
{
	Transform transform;
	Transform* parent = nullptr;
	glm::mat4 view;
	glm::mat4 projection;
	ProjectionInfo proj_info;

	Camera(ProjectionInfo info)
	{
		transform.position = glm::vec3(0.0f);
		transform.euler = glm::vec3(0.0f);
		transform.rotate_with_euler(glm::vec3(0.0f));
		set_projection(info);
	}

	void set_projection(ProjectionInfo info)
	{
		proj_info = info;

		if (proj_info.type == PROJ_PERSPECTIVE)
			projection = glm::perspective(glm::radians(info.perspective.fov), info.perspective.aspect_ratio, info.perspective.n, info.perspective.f);
		else
			projection = glm::ortho(info.othographic.l, info.othographic.r, info.othographic.b, info.othographic.t, info.othographic.n, info.othographic.f);
	}

	void update(float dt)
	{
		update_internal(dt);

		transform.update();

		glm::mat4 camera_transform = transform.model;

		if (parent)
			camera_transform *= parent->model;

		view = glm::inverse(camera_transform);
	}

	glm::vec3 forward()
	{
#ifdef GLM_FORCE_LEFT_HANDED
		return transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
#else
		return transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
#endif
	}

	glm::vec3 left()
	{
		return transform.rotation * glm::vec3(-1.0f, 0.0f, 0.0f);
	}

	virtual void update_internal(float dt) = 0;
};

struct OrbitCamera : public Camera
{
	OrbitCamera(float _distance, ProjectionInfo _info) : Camera(_info)
	{

	}

	void update_internal(float dt) override
	{

	}
};

struct FlythroughCamera : public Camera
{
	float speed;
	float max_pitch;
	float min_pitch;
	float damping;
	glm::vec3 rotation_delta = glm::vec3(0.0f);
	glm::vec3 forward_delta = glm::vec3(0.0f);
	glm::vec3 sideways_delta = glm::vec3(0.0f);

	FlythroughCamera(glm::vec3 _position, float _damping, float _max_pitch, float _min_pitch, ProjectionInfo _info) : Camera(_info), damping(_damping), max_pitch(_max_pitch), min_pitch(_min_pitch)
	{
		transform.position = _position;
	}

	void update_internal(float dt) override
	{
		transform.euler += rotation_delta;

		glm::quat pitch = glm::quat(glm::vec3(glm::radians(transform.euler.x), glm::radians(0.0f), glm::radians(0.0f)));
		glm::quat yaw = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(transform.euler.y), glm::radians(0.0f)));
		glm::quat roll = glm::quat(glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(transform.euler.z)));

		transform.rotation = yaw * pitch * roll;

		rotation_delta.x *= (1 - damping);
		rotation_delta.y *= (1 - damping);
		rotation_delta.z *= (1 - damping);

		transform.position += forward_delta;
		transform.position += sideways_delta;

		forward_delta *= (1 - damping);
		sideways_delta *= (1 - damping);
	}

	void rotate(glm::vec3 euler)
	{
		rotation_delta = euler;
	}

	void move_forwards(float amount)
	{
		forward_delta = forward() * amount;
	}

	void move_sideways(float amount)
	{
		sideways_delta = left() * amount;
	}
};