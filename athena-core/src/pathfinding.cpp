// ATHENA Core - Pathfinding Implementation
// A* algorithm with terrain-based movement costs
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/pathfinding.hpp"

#include <cmath>
#include <algorithm>
#include <limits>

namespace athena {

// =============================================================================
// A* Priority Queue Node
// =============================================================================

struct AStarNode {
    GridCoord coord;
    f64 f_score;  // g + h
    
    // Min-heap: lower f_score has higher priority
    bool operator>(const AStarNode& other) const {
        return f_score > other.f_score;
    }
};

// =============================================================================
// Pathfinder Implementation
// =============================================================================

Pathfinder::Pathfinder()
    : terrain_(nullptr)
    , config_()
    , initialized_(false)
{
}

Status Pathfinder::init(const TerrainSemantics* terrain, const PathfindingConfig& config) {
    if (!terrain) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Terrain cannot be null");
    }
    
    terrain_ = terrain;
    config_ = config;
    initialized_ = true;
    
    return Status();
}

void Pathfinder::reset() {
    // Nothing cached currently
}

void Pathfinder::configure(const PathfindingConfig& config) {
    config_ = config;
}

// =============================================================================
// Grid Conversion
// =============================================================================

GridCoord Pathfinder::world_to_grid(f64 x, f64 y) const {
    return GridCoord{
        static_cast<i32>(std::floor(x / config_.cell_size)),
        static_cast<i32>(std::floor(y / config_.cell_size))
    };
}

std::pair<f64, f64> Pathfinder::grid_to_world(GridCoord c) const {
    // Return center of cell
    return {
        (c.x + 0.5) * config_.cell_size,
        (c.y + 0.5) * config_.cell_size
    };
}

// =============================================================================
// A* Helpers
// =============================================================================

f64 Pathfinder::heuristic(GridCoord a, GridCoord b) const {
    // Euclidean distance (admissible heuristic)
    f64 dx = static_cast<f64>(b.x - a.x);
    f64 dy = static_cast<f64>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy) * config_.cell_size * config_.heuristic_weight;
}

std::vector<GridCoord> Pathfinder::get_neighbors(GridCoord c) const {
    std::vector<GridCoord> neighbors;
    neighbors.reserve(8);
    
    // 4 cardinal directions (always included)
    // Order is deterministic: N, E, S, W, then diagonals NE, SE, SW, NW
    neighbors.push_back({c.x, c.y + 1});      // North
    neighbors.push_back({c.x + 1, c.y});      // East
    neighbors.push_back({c.x, c.y - 1});      // South
    neighbors.push_back({c.x - 1, c.y});      // West
    
    if (config_.allow_diagonal) {
        neighbors.push_back({c.x + 1, c.y + 1});  // NE
        neighbors.push_back({c.x + 1, c.y - 1});  // SE
        neighbors.push_back({c.x - 1, c.y - 1});  // SW
        neighbors.push_back({c.x - 1, c.y + 1});  // NW
    }
    
    return neighbors;
}

f64 Pathfinder::edge_cost(GridCoord from, GridCoord to, Tick tick, MovementType move_type) const {
    // Get world coordinates of destination cell center
    auto [to_x, to_y] = grid_to_world(to);
    
    // Query terrain for movement cost
    MovementCost mc = terrain_->movement_cost(to_x, to_y, tick, move_type);
    
    // If impassable, return infinity
    if (!mc.passable || mc.cost >= 999.0) {
        return std::numeric_limits<f64>::infinity();
    }
    
    // Base distance
    f64 dx = static_cast<f64>(to.x - from.x);
    f64 dy = static_cast<f64>(to.y - from.y);
    bool diagonal = (dx != 0 && dy != 0);
    
    f64 base_distance = diagonal ? 
        config_.cell_size * config_.diagonal_cost : 
        config_.cell_size;
    
    // Total cost = distance * terrain multiplier
    return base_distance * mc.cost;
}

std::vector<std::pair<f64, f64>> Pathfinder::reconstruct_path(
    const std::unordered_map<GridCoord, GridCoord, GridCoordHash>& came_from,
    GridCoord current) const 
{
    std::vector<std::pair<f64, f64>> path;
    
    // Walk back from goal to start
    while (came_from.count(current) > 0) {
        path.push_back(grid_to_world(current));
        current = came_from.at(current);
    }
    // Add start
    path.push_back(grid_to_world(current));
    
    // Reverse to get start->goal order
    std::reverse(path.begin(), path.end());
    
    return path;
}

// =============================================================================
// Main A* Algorithm
// =============================================================================

PathResult Pathfinder::find_path(
    f64 start_x, f64 start_y,
    f64 goal_x, f64 goal_y,
    Tick tick,
    MovementType move_type) const 
{
    PathResult result;
    
    if (!initialized_) {
        return result;  // Not initialized
    }
    
    GridCoord start = world_to_grid(start_x, start_y);
    GridCoord goal = world_to_grid(goal_x, goal_y);
    
    // Trivial case: already at goal
    if (start == goal) {
        result.found = true;
        result.path.push_back({start_x, start_y});
        result.path.push_back({goal_x, goal_y});
        return result;
    }
    
    // Check if goal is passable
    MovementCost goal_cost = terrain_->movement_cost(goal_x, goal_y, tick, move_type);
    if (!goal_cost.passable) {
        return result;  // Goal unreachable
    }
    
    // A* data structures
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open_set;
    std::unordered_map<GridCoord, f64, GridCoordHash> g_score;
    std::unordered_map<GridCoord, GridCoord, GridCoordHash> came_from;
    std::unordered_map<GridCoord, bool, GridCoordHash> closed_set;
    
    // Initialize start node
    g_score[start] = 0.0;
    open_set.push({start, heuristic(start, goal)});
    
    u32 iterations = 0;
    
    while (!open_set.empty() && iterations < config_.max_iterations) {
        ++iterations;
        
        // Get node with lowest f_score
        AStarNode current_node = open_set.top();
        open_set.pop();
        GridCoord current = current_node.coord;
        
        // Skip if already processed
        if (closed_set.count(current) > 0) {
            continue;
        }
        closed_set[current] = true;
        result.nodes_explored++;
        
        // Goal reached
        if (current == goal) {
            result.found = true;
            result.path = reconstruct_path(came_from, current);
            result.iterations = iterations;
            
            // Replace first and last with exact coordinates
            if (!result.path.empty()) {
                result.path.front() = {start_x, start_y};
                result.path.back() = {goal_x, goal_y};
            }
            
            // Calculate statistics
            result.total_cost = g_score[goal];
            result.path_length_m = 0.0;
            for (size_t i = 1; i < result.path.size(); ++i) {
                f64 dx = result.path[i].first - result.path[i-1].first;
                f64 dy = result.path[i].second - result.path[i-1].second;
                result.path_length_m += std::sqrt(dx * dx + dy * dy);
            }
            if (result.path_length_m > 0.0) {
                result.average_cost = result.total_cost / result.path_length_m;
            }
            
            return result;
        }
        
        // Explore neighbors
        f64 current_g = g_score[current];
        
        for (const GridCoord& neighbor : get_neighbors(current)) {
            // Skip if already processed
            if (closed_set.count(neighbor) > 0) {
                continue;
            }
            
            // Calculate edge cost
            f64 edge = edge_cost(current, neighbor, tick, move_type);
            
            // Skip impassable
            if (std::isinf(edge)) {
                continue;
            }
            
            f64 tentative_g = current_g + edge;
            
            // Check if this path is better
            auto it = g_score.find(neighbor);
            if (it == g_score.end() || tentative_g < it->second) {
                // Better path found
                came_from[neighbor] = current;
                g_score[neighbor] = tentative_g;
                f64 f = tentative_g + heuristic(neighbor, goal);
                open_set.push({neighbor, f});
            }
        }
    }
    
    // No path found
    result.iterations = iterations;
    return result;
}

bool Pathfinder::path_exists(
    f64 start_x, f64 start_y,
    f64 goal_x, f64 goal_y,
    Tick tick,
    MovementType move_type) const 
{
    // Use regular pathfinding but could optimize later
    PathResult result = find_path(start_x, start_y, goal_x, goal_y, tick, move_type);
    return result.found;
}

f64 Pathfinder::compute_path_cost(
    const std::vector<std::pair<f64, f64>>& path,
    Tick tick,
    MovementType move_type) const 
{
    if (!initialized_ || path.size() < 2) {
        return 0.0;
    }
    
    f64 total = 0.0;
    
    for (size_t i = 1; i < path.size(); ++i) {
        f64 x1 = path[i-1].first;
        f64 y1 = path[i-1].second;
        f64 x2 = path[i].first;
        f64 y2 = path[i].second;
        
        // Distance
        f64 dx = x2 - x1;
        f64 dy = y2 - y1;
        f64 dist = std::sqrt(dx * dx + dy * dy);
        
        // Sample terrain at midpoint
        f64 mid_x = (x1 + x2) / 2.0;
        f64 mid_y = (y1 + y2) / 2.0;
        
        MovementCost mc = terrain_->movement_cost(mid_x, mid_y, tick, move_type);
        if (!mc.passable) {
            return std::numeric_limits<f64>::infinity();
        }
        
        total += dist * mc.cost;
    }
    
    return total;
}

std::vector<std::pair<f64, f64>> Pathfinder::smooth_path(
    const std::vector<std::pair<f64, f64>>& path,
    Tick tick,
    MovementType move_type) const 
{
    if (path.size() <= 2) {
        return path;
    }
    
    std::vector<std::pair<f64, f64>> smoothed;
    smoothed.push_back(path.front());
    
    size_t current = 0;
    
    while (current < path.size() - 1) {
        // Find furthest visible point
        size_t furthest = current + 1;
        
        for (size_t test = path.size() - 1; test > current + 1; --test) {
            // Check if direct path is passable
            f64 x1 = path[current].first;
            f64 y1 = path[current].second;
            f64 x2 = path[test].first;
            f64 y2 = path[test].second;
            
            // Sample along line
            f64 dist = std::sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
            int samples = static_cast<int>(dist / config_.cell_size) + 1;
            bool passable = true;
            
            for (int s = 1; s <= samples && passable; ++s) {
                f64 t = static_cast<f64>(s) / samples;
                f64 x = x1 + t * (x2 - x1);
                f64 y = y1 + t * (y2 - y1);
                
                MovementCost mc = terrain_->movement_cost(x, y, tick, move_type);
                if (!mc.passable) {
                    passable = false;
                }
            }
            
            if (passable) {
                furthest = test;
                break;
            }
        }
        
        smoothed.push_back(path[furthest]);
        current = furthest;
    }
    
    return smoothed;
}

// =============================================================================
// Convenience Function
// =============================================================================

PathResult find_path_simple(
    const TerrainSemantics* terrain,
    f64 start_x, f64 start_y,
    f64 goal_x, f64 goal_y,
    Tick tick,
    MovementType move_type,
    f64 cell_size)
{
    PathfindingConfig config;
    config.cell_size = cell_size;
    
    Pathfinder pf;
    auto status = pf.init(terrain, config);
    if (!status.ok()) {
        return PathResult();
    }
    
    return pf.find_path(start_x, start_y, goal_x, goal_y, tick, move_type);
}

}  // namespace athena
