// ATHENA Core - Pathfinding (A* with Terrain Costs)
// Contract: Find optimal paths using TerrainSemantics movement costs.
//
// ALGORITHM: A* with configurable grid resolution
// DETERMINISM: Same inputs = same path (ordered neighbor expansion)
//
// RULES:
// - Uses TerrainSemantics::movement_cost() for edge weights
// - Respects passability constraints
// - Grid-based discretization for efficiency
// - Heuristic: Euclidean distance (admissible)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_PATHFINDING_HPP
#define ATHENA_PATHFINDING_HPP

#include "athena/types.hpp"
#include "athena/terrain_semantics.hpp"

#include <vector>
#include <queue>
#include <unordered_map>

namespace athena {

// =============================================================================
// Configuration
// =============================================================================

struct PathfindingConfig {
    f64 cell_size = 100.0;          // Grid cell size in meters
    u32 max_iterations = 100000;    // Max A* iterations (prevent infinite)
    bool allow_diagonal = true;     // Allow 8-direction movement
    f64 diagonal_cost = 1.414;      // sqrt(2) for diagonal moves
    f64 heuristic_weight = 1.0;     // Weight for heuristic (1.0 = standard A*)
};

// =============================================================================
// Result
// =============================================================================

struct PathResult {
    std::vector<std::pair<f64, f64>> path;  // Waypoints (x, y)
    f64 total_cost = 0.0;                   // Sum of movement costs
    bool found = false;                     // Path exists?
    u32 iterations = 0;                     // A* iterations used
    u32 nodes_explored = 0;                 // Total nodes visited
    
    // Path statistics
    f64 path_length_m = 0.0;                // Geometric length
    f64 average_cost = 0.0;                 // total_cost / path_length_m
};

// =============================================================================
// Grid Node (internal)
// =============================================================================

struct GridCoord {
    i32 x;
    i32 y;
    
    bool operator==(const GridCoord& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const GridCoord& other) const {
        return !(*this == other);
    }
};

// Hash function for GridCoord
struct GridCoordHash {
    std::size_t operator()(const GridCoord& c) const {
        // Combine x and y into single hash
        return std::hash<i64>()(static_cast<i64>(c.x) << 32 | 
                               (static_cast<i64>(c.y) & 0xFFFFFFFF));
    }
};

// =============================================================================
// Pathfinder
// =============================================================================

class Pathfinder {
public:
    Pathfinder();
    
    /// Initialize with terrain semantics reference
    /// NOTE: Does not own terrain - caller must keep alive
    Status init(const TerrainSemantics* terrain, const PathfindingConfig& config);
    
    /// Reset state (clear caches)
    void reset();
    
    /// Update configuration
    void configure(const PathfindingConfig& config);
    
    // =========================================================================
    // Path Finding
    // =========================================================================
    
    /// Find path from start to goal
    /// @param start_x, start_y - Start position (meters)
    /// @param goal_x, goal_y - Goal position (meters)
    /// @param tick - Simulation tick (for time-varying terrain)
    /// @param move_type - Movement type (affects terrain costs)
    /// @return PathResult with path and statistics
    PathResult find_path(
        f64 start_x, f64 start_y,
        f64 goal_x, f64 goal_y,
        Tick tick,
        MovementType move_type) const;
    
    /// Check if path exists (faster than full pathfinding)
    bool path_exists(
        f64 start_x, f64 start_y,
        f64 goal_x, f64 goal_y,
        Tick tick,
        MovementType move_type) const;
    
    /// Compute cost of given path
    f64 compute_path_cost(
        const std::vector<std::pair<f64, f64>>& path,
        Tick tick,
        MovementType move_type) const;
    
    // =========================================================================
    // Utilities
    // =========================================================================
    
    /// Smooth path by removing redundant waypoints
    std::vector<std::pair<f64, f64>> smooth_path(
        const std::vector<std::pair<f64, f64>>& path,
        Tick tick,
        MovementType move_type) const;
    
    /// Get configuration
    const PathfindingConfig& config() const { return config_; }
    
    /// Check if initialized
    bool initialized() const { return initialized_; }

private:
    const TerrainSemantics* terrain_;
    PathfindingConfig config_;
    bool initialized_;
    
    // Grid conversion helpers
    GridCoord world_to_grid(f64 x, f64 y) const;
    std::pair<f64, f64> grid_to_world(GridCoord c) const;
    
    // A* helpers
    f64 heuristic(GridCoord a, GridCoord b) const;
    std::vector<GridCoord> get_neighbors(GridCoord c) const;
    f64 edge_cost(GridCoord from, GridCoord to, Tick tick, MovementType move_type) const;
    
    // Path reconstruction
    std::vector<std::pair<f64, f64>> reconstruct_path(
        const std::unordered_map<GridCoord, GridCoord, GridCoordHash>& came_from,
        GridCoord current) const;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/// Simple pathfinding without creating Pathfinder object
PathResult find_path_simple(
    const TerrainSemantics* terrain,
    f64 start_x, f64 start_y,
    f64 goal_x, f64 goal_y,
    Tick tick,
    MovementType move_type,
    f64 cell_size = 100.0);

}  // namespace athena

#endif  // ATHENA_PATHFINDING_HPP
