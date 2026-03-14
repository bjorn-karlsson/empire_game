#include "rendering/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

Camera::Camera(float aspectRatio)
    : m_aspectRatio(aspectRatio)
{
    m_targetPosition = m_target;
    m_targetDistance = m_distance;
    RecalculateVectors();
}

void Camera::Update(float deltaTime)
{
    // Smooth interpolation toward targets
    float lerpSpeed = 8.0f * deltaTime;
    lerpSpeed = std::min(lerpSpeed, 1.0f);

    m_distance = glm::mix(m_distance, m_targetDistance, lerpSpeed);
    m_yaw      = glm::mix(m_yaw, m_targetYaw, lerpSpeed);
    m_target   = glm::mix(m_target, m_targetPosition, lerpSpeed);

    RecalculateVectors();
}

void Camera::Pan(float dx, float dz)
{
    // Pan in the camera's local XZ plane (rotated by yaw)
    float rad = glm::radians(m_yaw);
    float cosY = cos(rad);
    float sinY = sin(rad);

    glm::vec3 right   = {cosY, 0.0f, sinY};
    glm::vec3 forward = {-sinY, 0.0f, cosY};

    // Scale pan speed with zoom level (pan faster when zoomed out)
    float zoomScale = m_distance / 15.0f;

    m_targetPosition += right   * dx * m_panSpeed * zoomScale;
    m_targetPosition += forward * dz * m_panSpeed * zoomScale;
}

void Camera::Zoom(float amount)
{
    m_targetDistance -= amount * m_zoomSpeed;
    m_targetDistance = glm::clamp(m_targetDistance, m_minDistance, m_maxDistance);
    // Pitch stays fixed — no angle change on zoom (ETW style)
}

void Camera::Rotate(float angleDeg)
{
    m_targetYaw += angleDeg;
}

void Camera::RecalculateVectors()
{
    // Calculate camera position on a sphere around the target point
    float pitchRad = glm::radians(m_pitch);
    float yawRad   = glm::radians(m_yaw);

    m_position.x = m_target.x + m_distance * cos(pitchRad) * sin(yawRad);
    m_position.y = m_target.y - m_distance * sin(pitchRad); // pitch is negative
    m_position.z = m_target.z + m_distance * cos(pitchRad) * cos(yawRad);
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::GetProjectionMatrix() const
{
    return glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
}

glm::mat4 Camera::GetViewProjectionMatrix() const
{
    return GetProjectionMatrix() * GetViewMatrix();
}

// ─── Screen to world ray ──────────────────────────────────────
glm::vec3 Camera::ScreenToWorldRay(float screenX, float screenY,
                                    float screenWidth, float screenHeight) const
{
    // Convert screen coords to normalized device coords (-1 to 1)
    float ndcX = (2.0f * screenX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY / screenHeight); // Flip Y

    // Unproject through inverse VP matrix
    glm::mat4 invVP = glm::inverse(GetViewProjectionMatrix());
    glm::vec4 worldNear = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 worldFar  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);

    worldNear /= worldNear.w;
    worldFar  /= worldFar.w;

    return glm::normalize(glm::vec3(worldFar - worldNear));
}

// ─── Screen to world plane (Y=0) ─────────────────────────────
glm::vec3 Camera::ScreenToWorldPlane(float screenX, float screenY,
                                      float screenWidth, float screenHeight) const
{
    glm::vec3 rayDir = ScreenToWorldRay(screenX, screenY, screenWidth, screenHeight);
    glm::vec3 rayOrigin = m_position;

    // Intersect with Y=0 plane
    if (std::abs(rayDir.y) < 0.0001f) {
        return glm::vec3(0); // Ray parallel to plane
    }

    float t = -rayOrigin.y / rayDir.y;
    if (t < 0) {
        return glm::vec3(0); // Intersection behind camera
    }

    return rayOrigin + rayDir * t;
}

void Camera::OnResize(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
}
