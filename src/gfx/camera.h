#pragma once

#include <glm/mat4x4.hpp>
#include "std/vec.h"

struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
};

struct Camera {
    void update();

	glm::mat4 getView();

    vec3 pos = vec3(0);
    vec3 fwd = vec3(0, 0, -1);
    vec3 up = vec3(0, 1, 0);
    vec3 right = vec3(1, 0, 0);
    vec3 world_up = vec3(0, 1, 0);
    float yaw = -90.f;
    float pitch = 0.f;
    float mov_speed = 0.5f;
    float rot_speed = 5.f;
};
