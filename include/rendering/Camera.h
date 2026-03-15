#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ─── Camera ───────────────────────────────────────────────────
// A strategy game camera: looks down at the map at an angle,
// can pan (WASD / right-click drag), zoom (scroll wheel),
// and rotate (Q/E). Similar to Total War's campaign camera.
class Camera {
public:
    Camera(float aspectRatio);

    // Per-frame update
    void Update(float deltaTime);

    // Controls
    void Pan(float dx, float dz);       // Move camera in world XZ plane
    void Zoom(float amount);             // Scroll wheel
    void Rotate(float angleDeg);         // Q/E rotation

    // Screen-to-world ray casting (for mouse picking)
    glm::vec3 ScreenToWorldRay(float screenX, float screenY,
                                float screenWidth, float screenHeight) const;

    // Get the world position where screen point intersects Y=0 plane
    glm::vec3 ScreenToWorldPlane(float screenX, float screenY,
                                  float screenWidth, float screenHeight) const;

    // Matrices
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;

    // Position
    glm::vec3 GetPosition() const { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }

    void OnResize(float aspectRatio);

    // Turn execution camera control
    void SetTarget(const glm::vec3& t) { m_target = t; m_targetPosition = t; }
    void SetDistance(float d) { m_distance = d; m_targetDistance = d; }
    float GetDistance() const { return m_distance; }

private:
    void RecalculateVectors();

    // Camera position and orientation
    glm::vec3 m_position  = {0.0f, 12.0f, 12.0f};
    glm::vec3 m_target    = {0.0f, 0.0f, 1.5f};    // Center of France
    glm::vec3 m_up        = {0.0f, 1.0f, 0.0f};

    // Orbit parameters
    float m_distance    = 18.0f;   // Distance from target
    float m_pitch       = -35.0f;  // Shallower angle like ETW
    float m_yaw         = 0.0f;    // Rotation around Y axis (degrees)

    // Zoom limits
    float m_minDistance  = 4.0f;
    float m_maxDistance  = 50.0f;

    // Pitch limits (don't go fully top-down or horizontal)
    float m_minPitch    = -80.0f;
    float m_maxPitch    = -20.0f;

    // Movement speed
    float m_panSpeed    = 10.0f;
    float m_zoomSpeed   = 3.0f;
    float m_rotateSpeed = 60.0f;

    // Smooth interpolation targets
    float m_targetDistance = 18.0f;
    float m_targetYaw     = 0.0f;
    glm::vec3 m_targetPosition = {0.0f, 0.0f, 0.0f};

    // Projection
    float m_aspectRatio = 16.0f / 9.0f;
    float m_fov         = 50.0f;  // wider FOV for ETW-style perspective
    float m_nearPlane   = 0.1f;
    float m_farPlane    = 200.0f;
};
