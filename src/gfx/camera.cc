#include "camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/input.h"
#include "std/logging.h"

void Camera::update() {
    const float dt = 1.f / 60.f;

    float off_right = isKeyDown(Key::D) - (float)isKeyDown(Key::A);
    float off_fwd   = isKeyDown(Key::W) - (float)isKeyDown(Key::S);
    float off_up    = isKeyDown(Key::E) - (float)isKeyDown(Key::Q);

    pos += right * off_right * dt * mov_speed;
    pos += fwd * off_fwd * dt * mov_speed;
    pos += world_up * off_up * dt * mov_speed;

    if (isMouseDown(Mouse::Right)) {
        vec2 mouse_rel = getMouseRel();

        yaw += mouse_rel.x * rot_speed * dt * -1;
        pitch += mouse_rel.y * rot_speed * dt;
    }

    float y = math::toRad(yaw);
    float p = math::toRad(pitch);

    fwd = {
        cosf(y) * cosf(p),
        sinf(p),
        sinf(y) * cosf(p),
    };
    fwd.norm();

    right = norm(cross(fwd, world_up));
    up    = norm(cross(right, fwd));
}

glm::mat4 Camera::getView() {
    const vec3 lookat = pos + fwd;
    return glm::lookAt(
        glm::vec3(pos.x, pos.y, pos.z),
        glm::vec3(lookat.x, lookat.y, lookat.z),
        glm::vec3(up.x, up.y, up.z)
    );
}
