#include "Arena.h"
#include <iostream>

int main() 
{
    // Create a 20x20 arena
    Arena arena(20, 20);
    
    // Load all robots from current directory
    if (!arena.load_robots(".")) {
        std::cerr << "Failed to load any robots!\n";
        return 1;
    }
    
    // Run the game
    arena.run_game();
    
    return 0;
}
