#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>

// ─── Input Manager ────────────────────────────────────────────
// Wraps GLFW input into a clean interface with press/release
// detection and mouse world-space projection.
class InputManager {
public:
    explicit InputManager(GLFWwindow* window);

    void Update(); // Call once per frame, before gameplay update

    // Keyboard
    bool IsKeyDown(int key) const;               // Held this frame
    bool IsKeyPressed(int key) const;             // Just pressed (edge)
    bool IsKeyReleased(int key) const;            // Just released (edge)

    // Mouse
    glm::vec2 GetMousePos() const { return m_mousePos; }
    glm::vec2 GetMouseDelta() const { return m_mouseDelta; }
    float     GetScrollDelta() const { return m_scrollDelta; }
    bool      IsMouseButtonDown(int button) const;
    bool      IsMouseButtonPressed(int button) const;
    bool      IsMouseButtonReleased(int button) const;

    // Camera control helpers
    bool IsDragging() const;  // Middle mouse or right mouse held

private:
    GLFWwindow* m_window;

    // Keyboard state: current frame and previous frame
    std::unordered_map<int, bool> m_keysCurrent;
    std::unordered_map<int, bool> m_keysPrevious;

    // Mouse state
    glm::vec2 m_mousePos   = {0, 0};
    glm::vec2 m_mousePrev  = {0, 0};
    glm::vec2 m_mouseDelta = {0, 0};
    float     m_scrollDelta = 0.0f;

    bool m_mouseButtons[3]     = {};  // left, right, middle
    bool m_mouseButtonsPrev[3] = {};

    // Scroll callback needs static access
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static float s_scrollAccumulator;
};
