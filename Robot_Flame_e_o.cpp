#include "RobotBase.h"
#include <cstdlib>
#include <ctime>
#include <set>
#include <cmath>
#include <limits>
#include <utility>

class Robot_Flame_e_o : public RobotBase 
{
private:
    bool target_found = false;
    int target_row = -1;
    int target_col = -1;

    int radar_direction = 1; // Radar scanning direction (1-8)
    bool fixed_radar = false; // Tracks whether radar is locked on a target
    const int max_range = 4; // Maximum range of the flamethrower
    std::set<std::pair<int, int>> obstacles_memory; // Memory of obstacles

    // Helper function to calculate Manhattan distance
    int calculate_distance(int row1, int col1, int row2, int col2) const 
    {
        return std::abs(row1 - row2) + std::abs(col1 - col2);
    }

    // Find the closest enemy from the radar results
    void find_closest_enemy(const std::vector<RadarObj>& radar_results, int current_row, int current_col) 
    {
        target_found = false;
        int closest_distance = std::numeric_limits<int>::max();

        for (const auto& obj : radar_results) 
        {
            if (obj.m_type == '@') // Enemy robot (Ratboy)
            {
                int distance = calculate_distance(current_row, current_col, obj.m_row, obj.m_col);
                if (distance <= max_range && distance < closest_distance) 
                {
                    closest_distance = distance;
                    target_row = obj.m_row;
                    target_col = obj.m_col;
                    target_found = true;
                    fixed_radar = true; // Lock the radar on this direction
                }
            }
        }
    }

    // Update the memory of obstacles
    void update_obstacle_memory(const std::vector<RadarObj>& radar_results) 
    {
        for (const auto& obj : radar_results) 
        {
            if (obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F') 
            {
                obstacles_memory.insert({obj.m_row, obj.m_col});
            }
        }
    }

    // Check if a cell is passable
    bool is_passable(int row, int col) const 
    {
        return obstacles_memory.find({row, col}) == obstacles_memory.end();
    }

public:
    Robot_Flame_e_o() : RobotBase(2, 5, flamethrower) 
    {
        std::srand(static_cast<unsigned int>(std::time(nullptr))); // Seed for random movement
    }

    // Set the radar direction for scanning
    virtual void get_radar_direction(int& radar_direction_out) override 
    {
        if (fixed_radar && target_found) 
        {
            // Keep scanning the same direction if a target is found
            radar_direction_out = radar_direction;
        } 
        else 
        {
            // Cycle through radar directions (1-8) if no target
            radar_direction_out = radar_direction;
            radar_direction = (radar_direction % 8) + 1; // Increment and wrap around
        }
    }

    // Process the radar results and update the target and obstacles
    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override 
    {
        target_found = false; // Reset target state
        int current_row, current_col;
        get_current_location(current_row, current_col);

        // Update obstacle memory
        update_obstacle_memory(radar_results);

        // Look for the closest enemy in the radar results
        find_closest_enemy(radar_results, current_row, current_col);

        if (!target_found) 
        {
            fixed_radar = false; // Unlock radar if no target is found
        }
    }

    // Get the location for shooting, if applicable
    virtual bool get_shot_location(int& shot_row, int& shot_col) override 
    {
        if (target_found) 
        {
            int current_row, current_col;
            get_current_location(current_row, current_col);

            // Check if target is within range
            if (calculate_distance(current_row, current_col, target_row, target_col) <= max_range) 
            {
                // Shoot at the target
                shot_row = target_row;
                shot_col = target_col;
                return true;
            } 
            else 
            {
                // Target is out of range
                target_found = false;
                fixed_radar = false;
            }
        }

        return false; // No valid target to shoot
    }

    // Get the movement direction and distance
    virtual void get_move_direction(int& move_direction, int& move_distance) override 
    {
        int current_row, current_col;
        get_current_location(current_row, current_col);

        if (target_found) 
        {
            // Move toward the target
            int row_step = (target_row > current_row) ? 1 : (target_row < current_row) ? -1 : 0;
            int col_step = (target_col > current_col) ? 1 : (target_col < current_col) ? -1 : 0;

            // Map row_step and col_step to direction (1-8)
            if (row_step == -1 && col_step == 0) move_direction = 1;      // N
            else if (row_step == -1 && col_step == 1) move_direction = 2;  // NE
            else if (row_step == 0 && col_step == 1) move_direction = 3;   // E
            else if (row_step == 1 && col_step == 1) move_direction = 4;   // SE
            else if (row_step == 1 && col_step == 0) move_direction = 5;   // S
            else if (row_step == 1 && col_step == -1) move_direction = 6;  // SW
            else if (row_step == 0 && col_step == -1) move_direction = 7;  // W
            else if (row_step == -1 && col_step == -1) move_direction = 8; // NW
            else move_direction = 5; // Default to South if at target
            
            move_distance = 1;
            return;
        }

        // Smarter movement if no target - try to move toward center
        int center_row = m_board_row_max / 2;
        int center_col = m_board_col_max / 2;
        int row_diff = center_row - current_row;
        int col_diff = center_col - current_col;
        
        if (std::abs(row_diff) > std::abs(col_diff) && row_diff != 0) {
            move_direction = (row_diff > 0) ? 5 : 1; // Down or Up
        } else if (col_diff != 0) {
            move_direction = (col_diff > 0) ? 3 : 7; // Right or Left
        } else {
            // At center or completely stuck - try random directions
            move_direction = (std::rand() % 8) + 1;
        }
        move_distance = 1;
    }
};

// Factory function to create Robot_Flame_e_o
extern "C" RobotBase* create_Flame_e_o() 
{
    return new Robot_Flame_e_o();
}