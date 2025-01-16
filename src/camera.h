#pragma once

#include <vk_types.h>
#include <SDL_events.h>

class Camera {
public:

    /**
     * @brief 处理输入事件
     * @param event 输入事件
     */
    void process_input(SDL_Event& event);

    /**
     * @brief 更新相机
     * @param dt 时间步长
     */
    void update(float dt);

    /**
     * @brief 获取视图矩阵
     * @return 视图矩阵
     */
    glm::mat4 get_view_matrix() const;

    /**
     * @brief 获取旋转矩阵
     * @return 旋转矩阵
     */
    glm::mat4 get_rotation_matrix() const;

    void set_position(glm::vec3 position);

private:
    glm::vec3 _position{0.f};
    glm::vec3 _velocity{0.f};
    float _pitch{0.f};
    float _yaw{0.f};

    // 记录按键状态
    std::unordered_map<SDL_Keycode, bool> _keyStates = {
        {SDLK_w, false},
        {SDLK_s, false},
        {SDLK_a, false},
        {SDLK_d, false},
    };


};
