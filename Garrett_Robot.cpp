#include "RobotBase.h"
#include <vector>
#include <string>
#include <cmath>

class Garrett_Robot : public RobotBase {
public:
    Garrett_Robot() {
        m_name = "GarrettBot";
        m_character = '+';
        m_weapon = grenade;
        m_armor = 4;
        m_move_speed = 3;
        m_grenades = 10;
    }

    virtual int get_radar_direction() override {
        return 0;
    }

    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        enemy_nearby = false;
        for (const auto& obj : radar_results) {
            if (obj.m_type == '!' || obj.m_type == '@') {
                enemy_row = obj.m_row;
                enemy_col = obj.m_col;
                enemy_nearby = true;
            }
        }
    }

    virtual bool get_shot_location(int& shot_row, int& shot_col) override {
        if (enemy_nearby && std::abs(enemy_row - m_row) <= 1 && std::abs(enemy_col - m_col) <= 1 && m_grenades > 0) {
            shot_row = enemy_row;
            shot_col = enemy_col;
            return true;
        }
        return false;
    }

    virtual void get_move_direction(int& move_direction, int& move_distance) override {
        if (enemy_nearby) {
            int dr = m_row - enemy_row;
            int dc = m_col - enemy_col;
            if (std::abs(dr) > std::abs(dc)) {
                move_direction = (dr > 0) ? 5 : 1;
            } else {
                move_direction = (dc > 0) ? 3 : 7;
            }
            move_distance = m_move_speed;
        } else {
            move_direction = 0;
            move_distance = 0;
        }
    }

    static RobotBase* create() { return new Garrett_Robot(); }

private:
    bool enemy_nearby = false;
    int enemy_row = -1, enemy_col = -1;
};

extern "C" RobotBase* create_robot() {
    return Garrett_Robot::create();
}
