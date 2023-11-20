#include "camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/input.h"

void Camera::update() {
    const float dt = 1.f / 60.f;

    float off_right = isKeyDown(Key::D) - (float)isKeyDown(Key::A);
    float off_fwd   = isKeyDown(Key::W) - (float)isKeyDown(Key::S);

    pos += right * off_right * dt;
    pos += fwd * off_fwd * dt;

    vec2 mouse_rel = getMouseRel();

    yaw += mouse_rel.x * mov_speed * dt;
    pitch += mouse_rel.y * mov_speed * dt;

    float y = math::toRad(yaw);
    float p = math::toRad(pitch);

    vec3 front = {
        cosf(y) * cosf(p),
        sinf(p),
        sinf(y) * cosf(p),
    };
    front.norm();

    right = norm(cross(front, world_up));
    up    = norm(cross(right, front));
}

glm::mat4 Camera::getView() {
    const vec3 lookat = pos + fwd;
    return glm::lookAt(
        glm::vec3(pos.x, pos.y, pos.z),
        glm::vec3(lookat.x, lookat.y, lookat.z),
        glm::vec3(up.x, up.y, up.z)
    );
}
