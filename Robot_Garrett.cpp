#include "RobotBase.h"
#include <vector>
#include <string>
#include <cmath>
#include <random>

// A passive robot that only throws a grenade when an enemy is adjacent
class Garrett_Robot : public RobotBase {
public:
    Garrett_Robot() : RobotBase(3, 4, grenade) {
        m_name = "GarrettBot";
        m_character = '+';
    }

    // Always request a local (8-neighbor) radar scan
    void get_radar_direction(int& radar_direction) override {
        radar_direction = 0;
    }

    void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        enemy_nearby = false;

        int cur_r, cur_c;
        get_current_location(cur_r, cur_c);

        int best_distance = 1'000'000;
        for (const auto& obj : radar_results) {
            if (is_enemy(obj.m_type)) {
                int dist = std::abs(obj.m_row - cur_r) + std::abs(obj.m_col - cur_c);
                if (dist < best_distance) {
                    best_distance = dist;
                    enemy_row = obj.m_row;
                    enemy_col = obj.m_col;
                    enemy_nearby = true;
                }
            }
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override {
        if (!enemy_nearby || get_grenades() <= 0) {
            return false;
        }

        int cur_r, cur_c;
        get_current_location(cur_r, cur_c);
        if (std::abs(enemy_row - cur_r) <= 1 && std::abs(enemy_col - cur_c) <= 1) {
            shot_row = enemy_row;
            shot_col = enemy_col;
            return true;
        }
        return false;
    }

    void get_move_direction(int& move_direction, int& move_distance) override {
        if (enemy_nearby) {
            int cur_r, cur_c;
            get_current_location(cur_r, cur_c);
            int dr = cur_r - enemy_row;
            int dc = cur_c - enemy_col;

            int step_r = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
            int step_c = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);

            // Map step vector to direction index
            move_direction = 0;
            for (int i = 1; i <= 8; i++) {
                if (directions[i].first == step_r && directions[i].second == step_c) {
                    move_direction = i;
                    break;
                }
            }

            move_distance = (move_direction == 0) ? 0 : get_move_speed();
            return;
        }

        // Wander when no enemies are visible to avoid endless stalemates
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dir_dist(1, 8);
        move_direction = dir_dist(rng);
        move_distance = 1; // small steps to reduce obstacle hits
    }

    static RobotBase* create() { return new Garrett_Robot(); }

private:
    bool enemy_nearby = false;
    int enemy_row = -1;
    int enemy_col = -1;

    bool is_enemy(char cell) const {
        // Ignore terrain and self; treat any other marker as a robot target
        if (cell == m_character) return false;
        if (cell == 'M' || cell == 'P' || cell == 'F' || cell == 'X') return false;
        return true;
    }
};

// Factory expected by the arena loader (derived from filename Robot_Garrett.cpp -> create_Garrett)
extern "C" RobotBase* create_Garrett() {
    return Garrett_Robot::create();
}

// Optional generic factory for compatibility with other loaders/tests
extern "C" RobotBase* create_robot() {
    return Garrett_Robot::create();
}
