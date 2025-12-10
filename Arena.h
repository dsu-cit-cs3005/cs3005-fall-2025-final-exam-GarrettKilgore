#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <thread>
#include <chrono>
#include "RobotBase.h"
#include "RadarObj.h"

enum CellType {
    EMPTY = '.',
    ROBOT = 'R',
    DEAD_ROBOT = 'X',
    MOUND = 'M',
    PIT = 'P',
    FLAMETHROWER = 'F'
};

struct RobotInfo {
    std::unique_ptr<RobotBase> robot;
    void* lib_handle;
    bool is_alive;
    bool in_pit;
    int stuck_count;
    int pit_turns;
    
    RobotInfo() : robot(nullptr), lib_handle(nullptr), is_alive(true), in_pit(false), stuck_count(0), pit_turns(0) {}
};

class Arena {
private:
    int m_rows;
    int m_cols;
    
    std::vector<std::vector<char>> m_board;
    
    std::vector<RobotInfo> m_robots;
    std::map<char, int> m_robot_symbol_to_index;
    
    int m_round;
    int m_alive_count;
    int m_max_rounds;  // Prevent infinite loops
    
    const std::string ROBOT_SYMBOLS = "!@#$%^&*+=?";
    
public:
    Arena(int rows = 20, int cols = 20);
    ~Arena();
    
    bool load_robots(const std::string& directory = ".");
    bool compile_robot(const std::string& cpp_filename);
    bool load_robot_library(const std::string& so_filename, const std::string& robot_name);
    
    void initialize_board();
    void place_obstacles();
    bool place_robot(int robot_index);
    
    void run_game();
    void run_round();
    void robot_turn(int robot_index);
    
    void handle_radar(int robot_index, bool verbose = true);
    void handle_movement(int robot_index, bool verbose = true);
    void handle_shooting(int robot_index, bool verbose = true);
    bool try_multiple_directions(int robot_index, int preferred_direction, int distance);
    void handle_stuck_robot(int robot_index);
    void handle_pit_escape(int robot_index, bool verbose = true);
    
    std::vector<RadarObj> scan_radar(int row, int col, int direction, int range = 5);
    
    void shoot_flamethrower(int shooter_row, int shooter_col, int direction);
    void shoot_railgun(int shooter_row, int shooter_col, int direction);
    void shoot_grenade(int target_row, int target_col);
    void shoot_hammer(int shooter_row, int shooter_col, int direction);
    void apply_damage(int robot_index, int damage, const std::string& source);
    
    bool can_move_to(int row, int col);
    bool move_robot(int robot_index, int new_row, int new_col);
    void check_obstacle_effects(int robot_index, int row, int col);
    
    void clear_robot_from_board(int robot_index);
    void place_robot_on_board(int robot_index, int row, int col);
    int get_robot_at(int row, int col);
    bool is_valid_position(int row, int col) const;
    
    void display_board() const;
    void display_robot_info(int robot_index) const;
    void print_separator() const;
    
    bool is_game_over() const;
    int get_winner() const;
    void announce_winner() const;
    
    void unload_robots();
};
