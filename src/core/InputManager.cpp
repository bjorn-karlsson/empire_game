#include "core/InputManager.h"

float InputManager::s_scrollAccumulator = 0.0f;

InputManager::InputManager(GLFWwindow* window) : m_window(window)
{
    glfwSetScrollCallback(window, ScrollCallback);
    // Get initial mouse position
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    m_mousePos  = {(float)mx, (float)my};
    m_mousePrev = m_mousePos;
}

void InputManager::Update()
{
    // Save previous state
    m_keysPrevious = m_keysCurrent;
    for (int i = 0; i < 3; i++)
        m_mouseButtonsPrev[i] = m_mouseButtons[i];

    // Poll keyboard — only track keys we've seen or commonly used
    // GLFW keys are queried on demand, so we sample the ones we care about
    int trackedKeys[] = {
        GLFW_KEY_ESCAPE, GLFW_KEY_SPACE, GLFW_KEY_ENTER,
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_L,
        GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_T,
        GLFW_KEY_F4, GLFW_KEY_LEFT_ALT, GLFW_KEY_LEFT_SHIFT,
        GLFW_KEY_TAB, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
        GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
    };
    for (int key : trackedKeys) {
        m_keysCurrent[key] = (glfwGetKey(m_window, key) == GLFW_PRESS);
    }

    // Poll mouse position
    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);
    m_mousePrev  = m_mousePos;
    m_mousePos   = {(float)mx, (float)my};
    m_mouseDelta = m_mousePos - m_mousePrev;

    // Poll mouse buttons
    m_mouseButtons[0] = (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    m_mouseButtons[1] = (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    m_mouseButtons[2] = (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);

    // Scroll
    m_scrollDelta = s_scrollAccumulator;
    s_scrollAccumulator = 0.0f;
}

// ─── Keyboard queries ─────────────────────────────────────────
bool InputManager::IsKeyDown(int key) const
{
    auto it = m_keysCurrent.find(key);
    return it != m_keysCurrent.end() && it->second;
}

bool InputManager::IsKeyPressed(int key) const
{
    auto curr = m_keysCurrent.find(key);
    auto prev = m_keysPrevious.find(key);
    bool currDown = (curr != m_keysCurrent.end() && curr->second);
    bool prevDown = (prev != m_keysPrevious.end() && prev->second);
    return currDown && !prevDown;
}

bool InputManager::IsKeyReleased(int key) const
{
    auto curr = m_keysCurrent.find(key);
    auto prev = m_keysPrevious.find(key);
    bool currDown = (curr != m_keysCurrent.end() && curr->second);
    bool prevDown = (prev != m_keysPrevious.end() && prev->second);
    return !currDown && prevDown;
}

// ─── Mouse queries ────────────────────────────────────────────
bool InputManager::IsMouseButtonDown(int button) const
{
    if (button < 0 || button > 2) return false;
    return m_mouseButtons[button];
}

bool InputManager::IsMouseButtonPressed(int button) const
{
    if (button < 0 || button > 2) return false;
    return m_mouseButtons[button] && !m_mouseButtonsPrev[button];
}

bool InputManager::IsMouseButtonReleased(int button) const
{
    if (button < 0 || button > 2) return false;
    return !m_mouseButtons[button] && m_mouseButtonsPrev[button];
}

bool InputManager::IsDragging() const
{
    return m_mouseButtons[1] || m_mouseButtons[2]; // right or middle
}

// ─── Scroll callback ──────────────────────────────────────────
void InputManager::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    s_scrollAccumulator += static_cast<float>(yoffset);
}
