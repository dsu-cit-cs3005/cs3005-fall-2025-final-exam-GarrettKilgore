# Arena Class Design - RobotWarz

## Overview
The Arena class is the core game engine that manages the turn-based robot combat simulation. It handles robot loading, board management, turn execution, combat resolution, and win condition checking.

---

## Class Structure

### Key Data Members

#### Board Management
- `m_board`: 2D vector of chars representing the game board
- `m_rows`, `m_cols`: Dimensions of the arena (default 20x20)

#### Robot Management
- `m_robots`: Vector of `RobotInfo` structs containing:
  - `unique_ptr<RobotBase>`: Ownership of robot object
  - `void* lib_handle`: Handle to loaded shared library
  - `is_alive`: Robot status
  - `in_pit`: Whether robot fell in a pit
- `m_robot_symbol_to_index`: Map from display symbols (!, @, #) to robot index

#### Game State
- `m_round`: Current round number
- `m_alive_count`: Number of living robots

---

## Key Functionality

### 1. Robot Loading (Dynamic Library Management)

**`load_robots()`**
- Scans directory for `Robot_*.cpp` files
- Compiles each to shared library (`.so`)
- Loads libraries using `dlopen()`
- Extracts factory functions with `dlsym()`
- Creates robot instances and stores in `m_robots` vector

**`compile_robot()`**
```cpp
g++ -shared -fPIC -o libRobotName.so Robot_Name.cpp RobotBase.cpp -std=c++17
```

**`load_robot_library()`**
- Opens `.so` file with `dlopen()`
- Finds `create_RobotName()` factory function
- Calls factory to instantiate robot
- Sets robot boundaries and assigns unique symbol
- Places robot on board

### 2. Game Loop

**`run_game()`**
1. Initialize board with obstacles
2. While not game over:
   - Run round
   - Increment round counter
3. Announce winner

**`run_round()`**
1. Display board
2. For each alive robot:
   - Execute `robot_turn()`

**`robot_turn()`**
1. Display robot info
2. Handle radar scan
3. Handle movement (if not in pit)
4. Handle shooting

### 3. Radar System

**`handle_radar()`**
1. Ask robot for scan direction via `get_radar_direction()`
2. Call `scan_radar()` to check cells in that direction
3. Pass results to robot via `process_radar_results()`

**`scan_radar()`**
- Scans up to 5 cells in specified direction
- Returns first non-empty cell as `RadarObj`
- Robot receives info about obstacles/enemies

### 4. Movement

**`handle_movement()`**
1. Ask robot for move direction/distance via `get_move_direction()`
2. Validate move doesn't exceed robot's speed
3. Calculate new position using `directions[]` array
4. Attempt move with `move_robot()`

**`move_robot()`**
1. Check if destination is valid (`can_move_to()`)
2. Clear robot from old position
3. Update robot's internal location
4. Check for obstacle effects (pit/flamethrower)
5. Place robot on new position

**Obstacles:**
- **Pit (P)**: Disables movement permanently
- **Flamethrower (F)**: Deals 15 damage
- **Mound (M)**: Blocks movement

### 5. Combat System

**`handle_shooting()`**
1. Ask robot if it wants to shoot via `get_shot_location()`
2. Route to appropriate weapon handler based on `WeaponType`

**Weapon Implementations:**

**Flamethrower** (`shoot_flamethrower`)
- 3 cells wide, 4 cells long rectangle
- Deals 15 damage per hit

**Railgun** (`shoot_railgun`)
- Straight line across entire arena
- Penetrates all obstacles and robots
- Deals 12 damage per robot hit

**Grenade** (`shoot_grenade`)
- 3x3 explosion area at target location
- Deals 20 damage
- Limited to 15 grenades

**Hammer** (`shoot_hammer`)
- Single adjacent cell
- Deals 25 damage
- Highest damage but shortest range

**`apply_damage()`**
1. Check for armor - reduces damage by 3 and loses 1 armor
2. Apply remaining damage to health via `take_damage()`
3. If health <= 0:
   - Mark robot as dead
   - Decrement alive count
   - Change board symbol to 'X'

### 6. Board Display

**`display_board()`**
```
     0  1  2  3  4  5  ...
 0   .  .  F  .  .  .
 1   .  R! .  .  M  .
 2   .  .  .  R@ .  .
```

**Board Symbols:**
- `.` = Empty
- `R!`, `R@`, `R#` = Live robots (unique symbols)
- `X` = Dead robot
- `M` = Mound (blocks movement)
- `P` = Pit (disables movement)
- `F` = Flamethrower (deals damage)

### 7. Win Condition

**`is_game_over()`**
- Game ends when <= 1 robot alive

**`announce_winner()`**
- Finds last surviving robot
- Displays winner info with trophy emoji

---

## Design Patterns Used

### 1. Factory Pattern
- Each robot `.so` exports a `create_RobotName()` function
- Arena calls factory to instantiate robots

### 2. Strategy Pattern
- Robots implement pure virtual functions from `RobotBase`
- Each robot has unique strategy/behavior

### 3. Template Method
- `robot_turn()` defines turn structure
- Robots fill in behavior through virtual functions

---

## Call Sequence Per Turn

```
robot_turn()
  ├─> display_robot_info()
  ├─> handle_radar()
  │     ├─> robot->get_radar_direction()
  │     ├─> scan_radar()
  │     └─> robot->process_radar_results()
  ├─> handle_movement()
  │     ├─> robot->get_move_direction()
  │     ├─> move_robot()
  │     │     ├─> can_move_to()
  │     │     ├─> clear_robot_from_board()
  │     │     ├─> check_obstacle_effects()
  │     │     └─> place_robot_on_board()
  └─> handle_shooting()
        ├─> robot->get_shot_location()
        └─> shoot_[weapon]()
              └─> apply_damage()
                    ├─> robot->reduce_armor()
                    └─> robot->take_damage()
```

---

## Memory Management

- **Smart Pointers**: `unique_ptr` for robot ownership
- **Shared Libraries**: `dlopen/dlclose` for library lifecycle
- **Destructor**: Unloads all libraries and cleans up resources

---

## Compilation

```bash
make              # Compiles Arena + main -> RobotWarz executable
./RobotWarz       # Arena compiles Robot_*.cpp files at runtime
```

---

## Extension Points

To add new features:
- **New obstacles**: Add to `CellType` enum and `place_obstacles()`
- **New weapons**: Add to `WeaponType` enum and weapon handlers
- **AI improvements**: Robots implement virtual functions differently
- **Board sizes**: Pass different dimensions to Arena constructor
- **Special rules**: Modify turn sequence in `robot_turn()`

---

## Critical Requirements Met

✅ Uses `RobotBase` as base class  
✅ Calls required robot functions per spec  
✅ Uses `RadarObj` for radar reporting  
✅ Loads robots as shared libraries  
✅ Turn-based simulation  
✅ All weapon types implemented  
✅ Obstacle mechanics working  
✅ Win condition detection  

---

This design provides a complete, extensible arena system that matches the specification requirements while maintaining clean separation of concerns.
