// ATHENA Core - CLI Runner
// Command-line interface for running simulations.
//
// Usage:
//   athena-cli [options] scenario.json
//
// Options:
//   -s, --seed <value>        Master seed (default: random)
//   -n, --iterations <count>  Monte Carlo iterations (default: 1)
//   -t, --ticks <count>       Max ticks per iteration (default: 168)
//   -o, --output <file>       Output file (JSON, default: stdout)
//   -q, --quiet               Suppress progress output
//   -v, --verbose             Verbose output
//   -h, --help                Show help
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena.hpp"
#include "athena/analysis.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>

using namespace athena;

// =============================================================================
// CLI Configuration
// =============================================================================

struct CliConfig {
    std::string scenario_file;
    std::string output_file;
    Seed master_seed;
    u32 num_iterations;
    u32 max_ticks;
    bool quiet;
    bool verbose;
    bool show_help;
    bool seed_set;
    
    CliConfig() 
        : scenario_file()
        , output_file()
        , master_seed(0)
        , num_iterations(1)
        , max_ticks(168)
        , quiet(false)
        , verbose(false)
        , show_help(false)
        , seed_set(false)
    {}
};

// =============================================================================
// Help Text
// =============================================================================

void print_help(const char* program) {
    std::cout << "ATHENA Core - Wargaming Engine\n";
    std::cout << "Usage: " << program << " [options] scenario.json\n\n";
    std::cout << "Options:\n";
    std::cout << "  -s, --seed <value>        Master seed (default: random)\n";
    std::cout << "  -n, --iterations <count>  Monte Carlo iterations (default: 1)\n";
    std::cout << "  -t, --ticks <count>       Max ticks per iteration (default: 168)\n";
    std::cout << "  -o, --output <file>       Output file (JSON, default: stdout)\n";
    std::cout << "  -q, --quiet               Suppress progress output\n";
    std::cout << "  -v, --verbose             Verbose output\n";
    std::cout << "  -h, --help                Show this help\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << program << " -s 12345 -n 100 -o results.json scenario.json\n";
}

// =============================================================================
// Argument Parsing
// =============================================================================

bool parse_args(int argc, char* argv[], CliConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            config.show_help = true;
            return true;
        }
        else if (arg == "-q" || arg == "--quiet") {
            config.quiet = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        }
        else if ((arg == "-s" || arg == "--seed") && i + 1 < argc) {
            config.master_seed = std::stoull(argv[++i]);
            config.seed_set = true;
        }
        else if ((arg == "-n" || arg == "--iterations") && i + 1 < argc) {
            config.num_iterations = static_cast<u32>(std::stoul(argv[++i]));
        }
        else if ((arg == "-t" || arg == "--ticks") && i + 1 < argc) {
            config.max_ticks = static_cast<u32>(std::stoul(argv[++i]));
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            config.output_file = argv[++i];
        }
        else if (arg[0] != '-') {
            config.scenario_file = arg;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Parse arguments
    CliConfig config;
    if (!parse_args(argc, argv, config)) {
        std::cerr << "Use -h for help.\n";
        return 1;
    }
    
    if (config.show_help) {
        print_help(argv[0]);
        return 0;
    }
    
    // Check scenario file
    if (config.scenario_file.empty()) {
        std::cerr << "Error: No scenario file specified.\n";
        std::cerr << "Use -h for help.\n";
        return 1;
    }
    
    // Generate random seed if not specified
    if (!config.seed_set) {
        config.master_seed = static_cast<Seed>(std::time(nullptr)) ^ 
                             (static_cast<Seed>(std::clock()) << 32);
    }
    
    // Print configuration
    if (!config.quiet) {
        std::cerr << "ATHENA Core CLI\n";
        std::cerr << "===============\n";
        std::cerr << "Scenario:    " << config.scenario_file << "\n";
        std::cerr << "Seed:        " << config.master_seed << "\n";
        std::cerr << "Iterations:  " << config.num_iterations << "\n";
        std::cerr << "Max ticks:   " << config.max_ticks << "\n";
        if (!config.output_file.empty()) {
            std::cerr << "Output:      " << config.output_file << "\n";
        }
        std::cerr << "\n";
    }
    
    // Load scenario
    if (!config.quiet) {
        std::cerr << "Loading scenario...\n";
    }
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(config.scenario_file);
    if (!scenario_result.ok()) {
        std::cerr << "Error loading scenario: " << loader.last_error() << "\n";
        return 1;
    }
    
    Scenario scenario = scenario_result.get();
    
    if (!config.quiet) {
        std::cerr << "Scenario loaded: " << scenario.name << "\n";
        std::cerr << "  Actors: " << scenario.actors.size() << "\n";
        
        // Count by side
        u32 blue_count = 0, red_count = 0, other_count = 0;
        for (const auto& actor : scenario.actors) {
            if (actor.side == "blue") blue_count++;
            else if (actor.side == "red") red_count++;
            else other_count++;
        }
        std::cerr << "  Blue:   " << blue_count << "\n";
        std::cerr << "  Red:    " << red_count << "\n";
        if (other_count > 0) {
            std::cerr << "  Other:  " << other_count << "\n";
        }
        std::cerr << "\n";
    }
    
    // Configure Monte Carlo
    analysis::BatchConfig batch_config;
    batch_config.master_seed = config.master_seed;
    batch_config.num_iterations = config.num_iterations;
    batch_config.max_ticks_per_iteration = config.max_ticks;
    batch_config.entity_capacity = 1024;
    
    // Use scenario temporal config if available
    if (scenario.temporal.max_ticks > 0) {
        batch_config.max_ticks_per_iteration = scenario.temporal.max_ticks;
    }
    
    // Create executor
    analysis::MonteCarloExecutor executor;
    executor.set_scenario(scenario);
    executor.configure(batch_config);
    
    // Run simulation
    if (!config.quiet) {
        std::cerr << "Running simulation...\n";
    }
    
    auto start_time = std::clock();
    executor.execute();
    auto end_time = std::clock();
    
    f64 elapsed_seconds = static_cast<f64>(end_time - start_time) / CLOCKS_PER_SEC;
    
    // Compute statistics
    auto stats = executor.compute_statistics();
    
    // Print summary
    if (!config.quiet) {
        std::cerr << "\n=== Results ===\n";
        std::cerr << "Completed: " << stats.completed_iterations << "/" 
                  << config.num_iterations << " iterations\n";
        std::cerr << "Duration:  " << elapsed_seconds << " seconds\n";
        
        if (stats.completed_iterations > 0) {
            std::cerr << "\nOutcomes:\n";
            std::cerr << "  Blue wins: " << stats.blue_wins << " (" 
                      << 100.0 * stats.blue_wins / stats.completed_iterations << "%)\n";
            std::cerr << "  Red wins:  " << stats.red_wins << " ("
                      << 100.0 * stats.red_wins / stats.completed_iterations << "%)\n";
            std::cerr << "  Draws:     " << stats.draws << " ("
                      << 100.0 * stats.draws / stats.completed_iterations << "%)\n";
            
            std::cerr << "\nBlue survival rate:\n";
            std::cerr << "  Mean:   " << stats.blue_survival_rate.mean << "\n";
            std::cerr << "  Stddev: " << stats.blue_survival_rate.stddev << "\n";
            std::cerr << "  Median: " << stats.blue_survival_rate.median << "\n";
            std::cerr << "  P5/P95: " << stats.blue_survival_rate.p5 << " / " 
                      << stats.blue_survival_rate.p95 << "\n";
            
            std::cerr << "\nRed survival rate:\n";
            std::cerr << "  Mean:   " << stats.red_survival_rate.mean << "\n";
            std::cerr << "  Stddev: " << stats.red_survival_rate.stddev << "\n";
            std::cerr << "  Median: " << stats.red_survival_rate.median << "\n";
            std::cerr << "  P5/P95: " << stats.red_survival_rate.p5 << " / "
                      << stats.red_survival_rate.p95 << "\n";
            
            std::cerr << "\nTicks to completion:\n";
            std::cerr << "  Mean:   " << stats.ticks_to_completion.mean << "\n";
            std::cerr << "  Median: " << stats.ticks_to_completion.median << "\n";
            std::cerr << "  P5/P95: " << stats.ticks_to_completion.p5 << " / "
                      << stats.ticks_to_completion.p95 << "\n";
        }
    }
    
    // Export results
    std::string json = executor.export_json();
    
    if (config.output_file.empty()) {
        // Output to stdout
        std::cout << json << "\n";
    } else {
        // Output to file
        std::ofstream out(config.output_file);
        if (!out) {
            std::cerr << "Error: Cannot write to " << config.output_file << "\n";
            return 1;
        }
        out << json << "\n";
        
        if (!config.quiet) {
            std::cerr << "\nResults written to: " << config.output_file << "\n";
        }
    }
    
    return 0;
}
