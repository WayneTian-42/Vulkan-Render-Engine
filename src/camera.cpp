#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::process_input(SDL_Event& event)
{
    // 事件处理
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        // 获取按键状态
        bool isPressed = (event.type == SDL_KEYDOWN);
        // 设置按键状态
        switch (event.key.keysym.sym) {
            case SDLK_w:
            case SDLK_s:
            case SDLK_a:
            case SDLK_d:
                _keyStates[event.key.keysym.sym] = isPressed;
                break;
        }
    }

    // 更新速度逻辑
    // 如果按下W和S，则速度为0
    _velocity.z = (_keyStates[SDLK_w] == _keyStates[SDLK_s]) ? 0.0f : 
    // 如果按下W，则速度为-1，如果按下S，则速度为1
        (_keyStates[SDLK_w] ? -1.0f : 1.0f);
    // 如果按下A和D，则速度为0
    _velocity.x = (_keyStates[SDLK_a] == _keyStates[SDLK_d]) ? 0.0f : 
        // 如果按下A，则速度为-1，如果按下D，则速度为1
        (_keyStates[SDLK_a] ? -1.0f : 1.0f);


    // 按下鼠标并且移动时，增加或减少俯仰角和偏航角
    if (event.type == SDL_MOUSEMOTION && event.motion.state == SDL_BUTTON_LEFT) {
        _pitch += static_cast<float>(event.motion.yrel) * 0.005f;
        _yaw -= static_cast<float>(event.motion.xrel) * 0.005f;
    }
}

void Camera::update(float dt)
{
    glm::mat4 cameraRotation = get_rotation_matrix();
    _position += glm::vec3(cameraRotation * glm::vec4(_velocity * 0.01f, 0.0f)) * dt;
}

glm::mat4 Camera::get_view_matrix() const
{
    // 计算相机的变换矩阵，然后求逆
    glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), _position) * get_rotation_matrix();
    return glm::inverse(cameraTransform);
}

glm::mat4 Camera::get_rotation_matrix() const
{
    // 创建绕X轴旋转的四元数（俯仰角）
    glm::quat pitchQuat = glm::angleAxis(_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    
    // 创建绕Y轴旋转的四元数（偏航角）
    glm::quat yawQuat = glm::angleAxis(_yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    // 将两个旋转组合，注意顺序：先偏航后俯仰
    // 这样可以避免万向节死锁问题
    return glm::toMat4(yawQuat * pitchQuat);  // 优化：直接组合四元数再转换矩阵
}

void Camera::set_position(glm::vec3 position)
{
    _position = position;
}

