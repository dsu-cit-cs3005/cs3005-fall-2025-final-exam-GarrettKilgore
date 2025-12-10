#include "Arena.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <dlfcn.h>
#include <cstdlib>
#include <algorithm>
#include <random>

namespace fs = std::filesystem;

// ===== CONSTRUCTOR/DESTRUCTOR =====

Arena::Arena(int rows, int cols) 
    : m_rows(rows), m_cols(cols), m_round(0), m_alive_count(0), m_max_rounds(1000) 
{
    // Initialize board with empty cells
    m_board.resize(m_rows, std::vector<char>(m_cols, EMPTY));
}

Arena::~Arena() 
{
    unload_robots();
}

// ===== ROBOT LOADING =====

bool Arena::load_robots(const std::string& directory) 
{
    std::cout << "\nLoading Robots...\n";
    
    // Find all Robot_*.cpp files
    for (const auto& entry : fs::directory_iterator(directory)) {
        std::string filename = entry.path().filename().string();
        
        // Check if it matches Robot_*.cpp pattern
        if (filename.find("Robot_") == 0 && filename.ends_with(".cpp")) {
            
            // Compile the robot
            if (!compile_robot(filename)) {
                std::cerr << "Failed to compile " << filename << std::endl;
                continue;
            }
            
            // Extract robot name (remove Robot_ prefix and .cpp suffix)
            std::string robot_name = filename.substr(6, filename.length() - 10);
            std::string so_filename = "lib" + robot_name + ".so";
            
            // Load the compiled library
            if (!load_robot_library(so_filename, robot_name)) {
                std::cerr << "Failed to load " << so_filename << std::endl;
                continue;
            }
        }
    }
    
    m_alive_count = m_robots.size();
    return m_robots.size() > 0;
}

bool Arena::compile_robot(const std::string& cpp_filename) 
{
    // Extract robot name
    std::string robot_name = cpp_filename.substr(6, cpp_filename.length() - 10);
    std::string so_filename = "lib" + robot_name + ".so";
    
    std::cout << "Compiling " << cpp_filename << " to " << so_filename << "...\n";
    
    // Build compilation command
    std::string compile_cmd = "g++ -shared -fPIC -o " + so_filename + 
                             " " + cpp_filename + " RobotBase.cpp -std=c++17";
    
    int result = system(compile_cmd.c_str());
    return result == 0;
}

bool Arena::load_robot_library(const std::string& so_filename, const std::string& robot_name) 
{
    // Open the shared library
    void* lib_handle = dlopen(("./" + so_filename).c_str(), RTLD_NOW);
    if (!lib_handle) {
        std::cerr << "dlopen error: " << dlerror() << std::endl;
        return false;
    }
    
    // Get the factory function
    std::string factory_name = "create_" + robot_name;
    RobotFactory factory = (RobotFactory)dlsym(lib_handle, factory_name.c_str());
    
    if (!factory) {
        std::cerr << "dlsym error: " << dlerror() << std::endl;
        dlclose(lib_handle);
        return false;
    }
    
    // Create the robot using the factory
    RobotBase* robot = factory();
    if (!robot) {
        std::cerr << "Factory failed to create robot\n";
        dlclose(lib_handle);
        return false;
    }
    
    // Set robot properties
    robot->m_name = robot_name;
    robot->set_boundaries(m_rows, m_cols);
    
    // Assign a unique symbol
    if (m_robots.size() < ROBOT_SYMBOLS.length()) {
        robot->m_character = ROBOT_SYMBOLS[m_robots.size()];
    }
    
    // Store robot info
    RobotInfo info;
    info.robot.reset(robot);
    info.lib_handle = lib_handle;
    info.is_alive = true;
    info.in_pit = false;
    
    int robot_index = m_robots.size();
    m_robots.push_back(std::move(info));
    m_robot_symbol_to_index[robot->m_character] = robot_index;
    
    // Place robot on board
    std::cout << "boundaries: " << m_rows << ", " << m_cols << std::endl;
    if (!place_robot(robot_index)) {
        std::cerr << "Failed to place robot on board\n";
        return false;
    }
    
    int row, col;
    robot->get_current_location(row, col);
    std::cout << "Loaded robot: " << robot_name << " at (" << row << ", " << col << ")\n";
    
    return true;
}

// ===== GAME SETUP =====

void Arena::initialize_board() 
{
    // Clear board
    for (int r = 0; r < m_rows; r++) {
        for (int c = 0; c < m_cols; c++) {
            m_board[r][c] = EMPTY;
        }
    }
    
    // Place obstacles
    place_obstacles();
    
    // Place robots on board
    for (size_t i = 0; i < m_robots.size(); i++) {
        if (m_robots[i].is_alive) {
            int row, col;
            m_robots[i].robot->get_current_location(row, col);
            m_board[row][col] = m_robots[i].robot->m_character;
        }
    }
}

void Arena::place_obstacles() 
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> row_dist(0, m_rows - 1);
    std::uniform_int_distribution<> col_dist(0, m_cols - 1);
    
    // Place random flamethrowers (5-8 obstacles)
    std::uniform_int_distribution<> flame_dist(5, 8);
    int flame_count = flame_dist(gen);
    for (int i = 0; i < flame_count; i++) {
        for (int attempt = 0; attempt < 20; attempt++) {
            int r = row_dist(gen);
            int c = col_dist(gen);
            if (m_board[r][c] == EMPTY) {
                m_board[r][c] = FLAMETHROWER;
                break;
            }
        }
    }
    
    // Place random pits (4-7 obstacles)
    std::uniform_int_distribution<> pit_dist(4, 7);
    int pit_count = pit_dist(gen);
    for (int i = 0; i < pit_count; i++) {
        for (int attempt = 0; attempt < 20; attempt++) {
            int r = row_dist(gen);
            int c = col_dist(gen);
            if (m_board[r][c] == EMPTY) {
                m_board[r][c] = PIT;
                break;
            }
        }
    }
    
    // Place random mounds (6-10 obstacles - most common)
    std::uniform_int_distribution<> mound_dist(6, 10);
    int mound_count = mound_dist(gen);
    for (int i = 0; i < mound_count; i++) {
        for (int attempt = 0; attempt < 20; attempt++) {
            int r = row_dist(gen);
            int c = col_dist(gen);
            if (m_board[r][c] == EMPTY) {
                m_board[r][c] = MOUND;
                break;
            }
        }
    }
}
bool Arena::place_robot(int robot_index) 
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> row_dist(0, m_rows - 1);
    std::uniform_int_distribution<> col_dist(0, m_cols - 1);
    
    // Helper lambda to count open neighbors
    auto count_open_neighbors = [this](int r, int c) -> int {
        int count = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r + dr;
                int nc = c + dc;
                if (is_valid_position(nr, nc) && m_board[nr][nc] == EMPTY) {
                    count++;
                }
            }
        }
        return count;
    };
    
    // Try to find empty spot with at least 3 open neighbors (max 150 attempts)
    for (int attempt = 0; attempt < 150; attempt++) {
        int r = row_dist(gen);
        int c = col_dist(gen);
        
        if (m_board[r][c] == EMPTY && count_open_neighbors(r, c) >= 3) {
            m_robots[robot_index].robot->move_to(r, c);
            m_board[r][c] = m_robots[robot_index].robot->m_character;
            return true;
        }
    }
    
    // Fallback: just find any empty spot
    for (int attempt = 0; attempt < 50; attempt++) {
        int r = row_dist(gen);
        int c = col_dist(gen);
        
        if (m_board[r][c] == EMPTY) {
            m_robots[robot_index].robot->move_to(r, c);
            m_board[r][c] = m_robots[robot_index].robot->m_character;
            return true;
        }
    }
    
    return false;
}

// ===== GAME LOOP =====

void Arena::run_game() 
{
    initialize_board();
    
    while (!is_game_over()) {
        run_round();
        m_round++;
    }
    
    announce_winner();
}

void Arena::run_round() 
{
    // Always display every round
    bool should_display = true;
    
    if (should_display) {
        std::cout << "\n=========== starting round " << m_round << " ===========\n";
        display_board();
        
        // Add a delay when displaying to make it readable
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    }
    
    // Each alive robot takes a turn
    for (size_t i = 0; i < m_robots.size(); i++) {
        if (m_robots[i].is_alive) {
            robot_turn(i);
        }
    }
}

void Arena::robot_turn(int robot_index) 
{
    RobotInfo& info = m_robots[robot_index];
    
    // Always display verbose output for every round
    bool verbose = true;
    
    if (verbose) {
        std::cout << "\n" << info.robot->m_name << " " << info.robot->m_character 
                  << " begins turn.\n";
        display_robot_info(robot_index);
    }
    
    // 1. Radar scan
    handle_radar(robot_index, verbose);
    
    // 2. Movement (with pit escape after 5 turns)
    if (!info.in_pit) {
        handle_movement(robot_index, verbose);
    } else {
        // Robot is stuck in pit - try to escape after 5 consecutive turns
        info.pit_turns++;
        if (info.pit_turns >= 5) {
            // Try to escape by teleporting out of pit
            handle_pit_escape(robot_index, verbose);
            info.pit_turns = 0;  // Reset counter after escape attempt
        } else if (verbose) {
            std::cout << info.robot->m_name << " is stuck in a pit! (" 
                      << info.pit_turns << "/5 turns)\n";
        }
    }
    
    // 3. Shooting
    handle_shooting(robot_index, verbose);
}

// ===== ROBOT ACTIONS =====

void Arena::handle_radar(int robot_index, bool verbose) 
{
    RobotBase* robot = m_robots[robot_index].robot.get();
    
    // Get robot location
    int row, col;
    robot->get_current_location(row, col);
    
    // Scan in all 8 directions (360-degree radar sweep)
    std::vector<RadarObj> all_results;
    for (int direction = 1; direction <= 8; direction++) {
        std::vector<RadarObj> results = scan_radar(row, col, direction);
        all_results.insert(all_results.end(), results.begin(), results.end());
    }
    
    // Report findings to robot
    if (verbose) {
        if (all_results.empty()) {
            std::cout << "  checking radar ...  found nothing. \n";
        } else {
            std::cout << "  checking radar ...  found '" << all_results[0].m_type 
                      << "' at (" << all_results[0].m_row << "," << all_results[0].m_col << ")\n";
        }
    }
    
    robot->process_radar_results(all_results);
}

void Arena::handle_movement(int robot_index, bool verbose) 
{
    RobotBase* robot = m_robots[robot_index].robot.get();
    
    int direction = 0;
    int distance = 0;
    robot->get_move_direction(direction, distance);
    
    if (direction == 0 || distance == 0) {
        return; // Robot chose not to move
    }
    
    // Get current location
    int current_row, current_col;
    robot->get_current_location(current_row, current_col);
    
    // Calculate new position
    int move_speed = robot->get_move_speed();
    distance = std::min(distance, move_speed);
    
    auto [dr, dc] = directions[direction];
    int new_row = current_row + (dr * distance);
    int new_col = current_col + (dc * distance);
    
    // Attempt to move
    bool moved = move_robot(robot_index, new_row, new_col);
    
    if (!moved) {
        // Try multiple directions if blocked
        moved = try_multiple_directions(robot_index, direction, distance);
        
        if (!moved) {
            // Track stuck count
            m_robots[robot_index].stuck_count++;
            
            // If stuck for 1+ turns, teleport to random spot (very low tolerance)
            if (m_robots[robot_index].stuck_count >= 1) {
                handle_stuck_robot(robot_index);
                moved = true;
            }
        }
    }
    
    if (moved) {
        m_robots[robot_index].stuck_count = 0;  // Reset stuck counter
        if (verbose) {
            int final_row, final_col;
            robot->get_current_location(final_row, final_col);
            std::cout << "Moving: " << robot->m_name << " moves to (" 
                      << final_row << "," << final_col << ").\n";
        }
    } else {
        if (verbose) {
            std::cout << "Movement blocked for " << robot->m_name << ".\n";
        }
    }
}

void Arena::handle_shooting(int robot_index, bool verbose) 
{
    RobotBase* robot = m_robots[robot_index].robot.get();
    
    int shot_row = -1, shot_col = -1;
    bool wants_to_shoot = robot->get_shot_location(shot_row, shot_col);
    
    if (!wants_to_shoot) {
        return;
    }
    
    int robot_row, robot_col;
    robot->get_current_location(robot_row, robot_col);
    
    WeaponType weapon = robot->get_weapon();
    
    if (verbose) {
        std::cout << "Shooting: " << weapon << " ";
    }
    
    switch (weapon) {
        case flamethrower: {
            // Determine direction to target
            int dr = (shot_row > robot_row) ? 1 : (shot_row < robot_row) ? -1 : 0;
            int dc = (shot_col > robot_col) ? 1 : (shot_col < robot_col) ? -1 : 0;
            
            // Find direction index
            int direction = 0;
            for (int i = 1; i <= 8; i++) {
                if (directions[i].first == dr && directions[i].second == dc) {
                    direction = i;
                    break;
                }
            }
            shoot_flamethrower(robot_row, robot_col, direction);
            break;
        }
        case railgun: {
            int dr = (shot_row > robot_row) ? 1 : (shot_row < robot_row) ? -1 : 0;
            int dc = (shot_col > robot_col) ? 1 : (shot_col < robot_col) ? -1 : 0;
            int direction = 0;
            for (int i = 1; i <= 8; i++) {
                if (directions[i].first == dr && directions[i].second == dc) {
                    direction = i;
                    break;
                }
            }
            shoot_railgun(robot_row, robot_col, direction);
            break;
        }
        case grenade:
            if (robot->get_grenades() > 0) {
                shoot_grenade(shot_row, shot_col);
                robot->decrement_grenades();
            } else {
                std::cout << "Out of grenades!\n";
            }
            break;
        case hammer: {
            // Hammer hits adjacent cell in direction of target
            int dr = (shot_row > robot_row) ? 1 : (shot_row < robot_row) ? -1 : 0;
            int dc = (shot_col > robot_col) ? 1 : (shot_col < robot_col) ? -1 : 0;
            int direction = 0;
            for (int i = 1; i <= 8; i++) {
                if (directions[i].first == dr && directions[i].second == dc) {
                    direction = i;
                    break;
                }
            }
            shoot_hammer(robot_row, robot_col, direction);
            break;
        }
    }
}

// ===== RADAR SYSTEM =====

std::vector<RadarObj> Arena::scan_radar(int row, int col, int direction, int range) 
{
    std::vector<RadarObj> results;
    
    auto [dr, dc] = directions[direction];
    
    // Scan in specified direction up to range
    for (int dist = 1; dist <= range; dist++) {
        int check_row = row + (dr * dist);
        int check_col = col + (dc * dist);
        
        if (!is_valid_position(check_row, check_col)) {
            break;
        }
        
        char cell = m_board[check_row][check_col];
        
        if (cell != EMPTY) {
            results.emplace_back(cell, check_row, check_col);
            break; // Only return first non-empty object
        }
    }
    
    return results;
}

// ===== SHOOTING/DAMAGE =====

void Arena::shoot_flamethrower(int shooter_row, int shooter_col, int direction) 
{
    auto [dr, dc] = directions[direction];
    
    // Flamethrower: 3 wide, 4 long box
    for (int dist = 1; dist <= 4; dist++) {
        int center_row = shooter_row + (dr * dist);
        int center_col = shooter_col + (dc * dist);
        
        // Hit 3x1 perpendicular to direction
        for (int offset = -1; offset <= 1; offset++) {
            int hit_row = center_row + (dc * offset); // Perpendicular
            int hit_col = center_col + (dr * offset);
            
            if (is_valid_position(hit_row, hit_col)) {
                int target = get_robot_at(hit_row, hit_col);
                if (target >= 0) {
                    apply_damage(target, 15, "flamethrower");
                }
            }
        }
    }
}

void Arena::shoot_railgun(int shooter_row, int shooter_col, int direction) 
{
    auto [dr, dc] = directions[direction];
    
    // Railgun: straight line across entire arena
    int current_row = shooter_row + dr;
    int current_col = shooter_col + dc;
    
    while (is_valid_position(current_row, current_col)) {
        int target = get_robot_at(current_row, current_col);
        if (target >= 0) {
            apply_damage(target, 12, "railgun");
        }
        
        current_row += dr;
        current_col += dc;
    }
}

void Arena::shoot_grenade(int target_row, int target_col) 
{
    // Grenade: 3x3 area
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int hit_row = target_row + dr;
            int hit_col = target_col + dc;
            
            if (is_valid_position(hit_row, hit_col)) {
                int target = get_robot_at(hit_row, hit_col);
                if (target >= 0) {
                    apply_damage(target, 20, "grenade");
                }
            }
        }
    }
}

void Arena::shoot_hammer(int shooter_row, int shooter_col, int direction) 
{
    auto [dr, dc] = directions[direction];
    
    // Hammer: just one adjacent cell
    int hit_row = shooter_row + dr;
    int hit_col = shooter_col + dc;
    
    if (is_valid_position(hit_row, hit_col)) {
        int target = get_robot_at(hit_row, hit_col);
        if (target >= 0) {
            apply_damage(target, 25, "hammer");
        }
    }
}

void Arena::apply_damage(int robot_index, int damage, const std::string& /*source*/) 
{
    RobotInfo& info = m_robots[robot_index];
    
    // Reduce armor first
    if (info.robot->get_armor() > 0) {
        info.robot->reduce_armor(1);
        damage -= 3; // Armor reduces damage
    }
    
    int remaining_health = info.robot->take_damage(damage);
    
    std::cout << info.robot->m_name << " takes " << damage 
              << " damage. Health: " << remaining_health << "\n";
    
    if (remaining_health <= 0) {
        info.is_alive = false;
        m_alive_count--;
        
        int row, col;
        info.robot->get_current_location(row, col);
        m_board[row][col] = DEAD_ROBOT;
        
        std::cout << info.robot->m_name << " is DESTROYED!\n";
    }
}

// ===== MOVEMENT & COLLISION =====

bool Arena::can_move_to(int row, int col) 
{
    if (!is_valid_position(row, col)) {
        return false;
    }
    
    char cell = m_board[row][col];
    
    // Can move to empty spaces, pits, and flamethrowers (take damage but can move)
    // Can't move through mounds or other robots
    if (cell == MOUND) {
        return false;
    }
    
    // Check if it's another robot (characters ! and @)
    if (cell == '!' || cell == '@') {
        return false;
    }
    
    return true;
}

bool Arena::move_robot(int robot_index, int new_row, int new_col) 
{
    if (!can_move_to(new_row, new_col)) {
        return false;
    }
    
    // Clear old position
    clear_robot_from_board(robot_index);
    
    // Update robot location
    m_robots[robot_index].robot->move_to(new_row, new_col);
    
    // Check for obstacle effects
    check_obstacle_effects(robot_index, new_row, new_col);
    
    // Place on new position
    place_robot_on_board(robot_index, new_row, new_col);
    
    return true;
}

void Arena::check_obstacle_effects(int robot_index, int row, int col) 
{
    char cell = m_board[row][col];
    RobotInfo& info = m_robots[robot_index];
    
    if (cell == PIT) {
        std::cout << info.robot->m_name << " fell into a PIT!\n";
        info.in_pit = true;
        info.robot->disable_movement();
    }
    else if (cell == FLAMETHROWER) {
        std::cout << info.robot->m_name << " triggered a FLAMETHROWER!\n";
        apply_damage(robot_index, 15, "obstacle flamethrower");
    }
}

// ===== BOARD UTILITIES =====

void Arena::clear_robot_from_board(int robot_index) 
{
    int row, col;
    m_robots[robot_index].robot->get_current_location(row, col);
    
    if (is_valid_position(row, col)) {
        char cell = m_board[row][col];
        // Only clear if it's this robot
        if (cell == m_robots[robot_index].robot->m_character) {
            m_board[row][col] = EMPTY;
        }
    }
}

void Arena::place_robot_on_board(int robot_index, int row, int col) 
{
    if (is_valid_position(row, col)) {
        m_board[row][col] = m_robots[robot_index].robot->m_character;
    }
}

int Arena::get_robot_at(int row, int col) 
{
    if (!is_valid_position(row, col)) {
        return -1;
    }
    
    char cell = m_board[row][col];
    
    // Check if it's a robot symbol
    auto it = m_robot_symbol_to_index.find(cell);
    if (it != m_robot_symbol_to_index.end()) {
        return it->second;
    }
    
    return -1;
}

bool Arena::is_valid_position(int row, int col) const 
{
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

// ===== DISPLAY =====

void Arena::display_board() const 
{
    // Print column numbers
    std::cout << "    ";
    for (int c = 0; c < m_cols; c++) {
        std::cout << std::setw(3) << c;
    }
    std::cout << "\n\n";
    
    // Print board
    for (int r = 0; r < m_rows; r++) {
        std::cout << std::setw(2) << r << "  ";
        for (int c = 0; c < m_cols; c++) {
            std::cout << " " << m_board[r][c] << " ";
        }
        std::cout << "\n\n";
    }
}

void Arena::display_robot_info(int robot_index) const 
{
    const RobotBase* robot = m_robots[robot_index].robot.get();
    if (robot) {
        std::cout << robot->print_stats();
    }
}

void Arena::print_separator() const 
{
    std::cout << "========================================\n";
}

// ===== GAME STATE =====

bool Arena::is_game_over() const 
{
    return m_alive_count <= 1 || m_round >= m_max_rounds;
}

int Arena::get_winner() const 
{
    for (size_t i = 0; i < m_robots.size(); i++) {
        if (m_robots[i].is_alive) {
            return i;
        }
    }
    return -1;
}

void Arena::announce_winner() const 
{
    if (m_round >= m_max_rounds) {
        print_separator();
        std::cout << "\n⏱️  TIMEOUT: Maximum rounds (" << m_max_rounds << ") reached!\n";
        std::cout << "Survivors:\n";
        for (size_t i = 0; i < m_robots.size(); i++) {
            if (m_robots[i].is_alive && m_robots[i].robot) {
                std::cout << "  - " << m_robots[i].robot->m_name 
                          << " (Health: " << m_robots[i].robot->get_health() << ")\n";
            }
        }
        print_separator();
        return;
    }
    
    int winner = get_winner();
    if (winner >= 0 && m_robots[winner].robot) {
        print_separator();
        std::cout << "\n🏆 WINNER: " << m_robots[winner].robot->m_name 
                  << " " << m_robots[winner].robot->m_character << " 🏆\n";
        display_robot_info(winner);
        print_separator();
    } else {
        std::cout << "\nNo winner - all robots destroyed!\n";
    }
}

// ===== MOVEMENT HELPERS =====

bool Arena::try_multiple_directions(int robot_index, int preferred_direction, int distance) 
{
    RobotBase* robot = m_robots[robot_index].robot.get();
    int current_row, current_col;
    robot->get_current_location(current_row, current_col);
    
    // Try all 8 directions in order: preferred, adjacent, opposite, etc.
    std::vector<int> try_order;
    
    // Start with preferred
    try_order.push_back(preferred_direction);
    
    // Try directions adjacent to preferred
    int left = (preferred_direction == 1) ? 8 : preferred_direction - 1;
    int right = (preferred_direction == 8) ? 1 : preferred_direction + 1;
    try_order.push_back(left);
    try_order.push_back(right);
    
    // Try remaining directions
    for (int dir = 1; dir <= 8; dir++) {
        if (dir != preferred_direction && dir != left && dir != right) {
            try_order.push_back(dir);
        }
    }
    
    // Try each direction with original distance, then with distance=1
    for (int dist = distance; dist >= 1; dist--) {
        for (int dir : try_order) {
            auto [dr, dc] = directions[dir];
            int new_row = current_row + (dr * dist);
            int new_col = current_col + (dc * dist);
            
            if (move_robot(robot_index, new_row, new_col)) {
                return true;
            }
        }
    }
    
    // Last resort: try all directions with distance 1 in random order
    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<int> all_dirs = {1, 2, 3, 4, 5, 6, 7, 8};
    std::shuffle(all_dirs.begin(), all_dirs.end(), gen);
    
    for (int dir : all_dirs) {
        auto [dr, dc] = directions[dir];
        int new_row = current_row + dr;
        int new_col = current_col + dc;
        
        if (move_robot(robot_index, new_row, new_col)) {
            return true;
        }
    }
    
    return false;
}

void Arena::handle_stuck_robot(int robot_index) 
{
    RobotBase* robot = m_robots[robot_index].robot.get();
    
    // Clear robot from current position
    clear_robot_from_board(robot_index);
    
    // Try to teleport to an empty spot near the center
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> row_dist(0, m_rows - 1);
    std::uniform_int_distribution<> col_dist(0, m_cols - 1);
    
    // First try center area (50x50 attempts in center)
    int center_row = m_rows / 2;
    int center_col = m_cols / 2;
    int center_range = m_rows / 3;  // Search within 1/3 of board from center
    
    for (int attempt = 0; attempt < 100; attempt++) {
        int r, c;
        if (attempt < 50) {
            // Try within center range first
            r = center_row + (int)(row_dist(gen) % (2 * center_range + 1)) - center_range;
            c = center_col + (int)(col_dist(gen) % (2 * center_range + 1)) - center_range;
            r = std::max(0, std::min(m_rows - 1, r));
            c = std::max(0, std::min(m_cols - 1, c));
        } else {
            // If center fails, try random locations
            r = row_dist(gen);
            c = col_dist(gen);
        }
        
        if (m_board[r][c] == EMPTY) {
            robot->move_to(r, c);
            place_robot_on_board(robot_index, r, c);
            m_robots[robot_index].stuck_count = 0;
            std::cout << "🔄 " << robot->m_name << " teleported to (" << r << "," << c << ") to escape!\n";
            return;
        }
    }
    
    // If we still couldn't find a spot, just reset and hope next round is better
    m_robots[robot_index].stuck_count = 0;
}

void Arena::handle_pit_escape(int robot_index, bool verbose) 
{
    RobotBase* robot = m_robots[robot_index].robot.get();
    RobotInfo& info = m_robots[robot_index];
    
    // Get current location
    int current_row, current_col;
    robot->get_current_location(current_row, current_col);
    
    // Try to move out of pit (adjacent cells first)
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Try all 8 adjacent directions from pit
    std::vector<int> escape_dirs = {1, 2, 3, 4, 5, 6, 7, 8};
    std::shuffle(escape_dirs.begin(), escape_dirs.end(), gen);
    
    for (int dir : escape_dirs) {
        auto [dr, dc] = directions[dir];
        int new_row = current_row + dr;
        int new_col = current_col + dc;
        
        if (can_move_to(new_row, new_col)) {
            // Clear old position
            clear_robot_from_board(robot_index);
            
            // Move out of pit
            robot->move_to(new_row, new_col);
            place_robot_on_board(robot_index, new_row, new_col);
            
            // Escaped pit!
            info.in_pit = false;
            if (verbose) {
                std::cout << "💨 " << robot->m_name << " escaped the pit!\n";
            }
            return;
        }
    }
    
    // If stuck, teleport randomly as last resort
    std::uniform_int_distribution<> row_dist(0, m_rows - 1);
    std::uniform_int_distribution<> col_dist(0, m_cols - 1);
    
    for (int attempt = 0; attempt < 50; attempt++) {
        int r = row_dist(gen);
        int c = col_dist(gen);
        
        if (m_board[r][c] == EMPTY) {
            clear_robot_from_board(robot_index);
            robot->move_to(r, c);
            place_robot_on_board(robot_index, r, c);
            info.in_pit = false;
            if (verbose) {
                std::cout << "🚀 " << robot->m_name << " teleported out of pit to (" << r << "," << c << ")!\n";
            }
            return;
        }
    }
}

// ===== CLEANUP =====

void Arena::unload_robots() 
{
    // First, explicitly delete all robot objects while their code is still loaded
    for (auto& info : m_robots) {
        info.robot.reset();  // Destroy robot before unloading its code
    }
    
    // Now safe to close the libraries
    for (auto& info : m_robots) {
        if (info.lib_handle) {
            dlclose(info.lib_handle);
            info.lib_handle = nullptr;
        }
    }
    m_robots.clear();
}
