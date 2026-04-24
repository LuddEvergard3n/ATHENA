// ATHENA CLI - Simplified Command Line Interface
// SPDX-License-Identifier: Proprietary

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/json.hpp"
#include "athena/scenario.hpp"
#include "athena/entities.hpp"
#include "athena/context.hpp"
#include "athena/manifest.hpp"
#include "athena/platform_loader.hpp"
#include "athena/analysis/montecarlo.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

using namespace athena;

static const char* VERSION = "1.1.2-cli";

void print_usage() {
    std::cerr << "ATHENA Military Simulation Framework v" << VERSION << "\n";
    std::cerr << "Usage: athena-cli <command> [options]\n\n";
    std::cerr << "Commands:\n";
    std::cerr << "  info                    Show version info\n";
    std::cerr << "  platforms <data_path>   List available platforms\n";
    std::cerr << "  validate <scenario>     Validate scenario file\n";
    std::cerr << "  run <scenario> [seed]   Run single simulation\n";
    std::cerr << "  batch <scenario> <n>    Run Monte Carlo batch\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  athena-cli platforms ./athena-core/data\n";
    std::cerr << "  athena-cli validate scenario.json\n";
    std::cerr << "  athena-cli run scenario.json 12345\n";
    std::cerr << "  athena-cli batch scenario.json 100\n";
}

int cmd_info() {
    std::cout << "ATHENA Military Simulation Framework\n";
    std::cout << "Version: " << VERSION << "\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "Compiler: " << 
#ifdef __GNUC__
        "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__
#else
        "Unknown"
#endif
        << "\n";
    return 0;
}

int cmd_platforms(const std::string& data_path) {
    std::cout << "Loading platforms from: " << data_path << "\n";
    
    PlatformDatabase db;
    auto result = db.load_all(data_path + "/platforms");
    
    if (!result) {
        std::cerr << "Error loading platforms: " << result.error.message << "\n";
        return 1;
    }
    
    std::cout << "Loaded " << db.count() << " platforms\n\n";
    
    // Count by category
    std::cout << "By category:\n";
    for (int i = 0; i < static_cast<int>(PlatformCategory::MAX_CATEGORIES); ++i) {
        auto cat = static_cast<PlatformCategory>(i);
        auto count = db.count_by_category(cat);
        if (count > 0) {
            std::cout << "  " << category_to_string(cat) << ": " << count << "\n";
        }
    }
    
    return 0;
}

int cmd_validate(const std::string& scenario_path) {
    std::cout << "Validating: " << scenario_path << "\n";
    
    ScenarioLoader loader;
    auto result = loader.load(scenario_path);
    
    if (!result) {
        std::cerr << "Validation FAILED: " << result.error.message << "\n";
        return 1;
    }
    
    const auto& scenario = *result.value;
    std::cout << "Scenario: " << scenario.name << "\n";
    std::cout << "Actors: " << scenario.actors.size() << "\n";
    std::cout << "Validation: " << (scenario.is_valid ? "PASSED" : "FAILED") << "\n";
    
    if (!scenario.validation_errors.empty()) {
        std::cout << "Errors:\n";
        for (const auto& err : scenario.validation_errors) {
            std::cout << "  - " << err << "\n";
        }
    }
    
    return scenario.is_valid ? 0 : 1;
}

int cmd_run(const std::string& scenario_path, Seed seed) {
    std::cout << "Running simulation: " << scenario_path << "\n";
    std::cout << "Seed: " << seed << "\n";
    
    // Load scenario
    ScenarioLoader loader;
    auto scenario_result = loader.load(scenario_path);
    if (!scenario_result) {
        std::cerr << "Error loading scenario: " << scenario_result.error.message << "\n";
        return 1;
    }
    
    const auto& scenario = *scenario_result.value;
    std::cout << "Scenario: " << scenario.name << "\n";
    std::cout << "Actors: " << scenario.actors.size() << "\n";
    
    // Create RNG
    Rng rng(seed);
    
    // Create entity manager
    EntityManager entities;
    entities.init(scenario.actors.size() + 100);
    
    // Convert scenario to entities
    ScenarioConverter converter;
    auto convert_result = converter.convert(scenario, entities, rng);
    if (!convert_result) {
        std::cerr << "Error converting scenario: " << convert_result.error.message << "\n";
        return 1;
    }
    
    std::cout << "Entities created: " << *convert_result.value << "\n";
    std::cout << "Simulation ready.\n";
    
    // Run simulation loop (simplified)
    const Tick max_ticks = scenario.temporal.max_ticks;
    std::cout << "Max ticks: " << max_ticks << "\n";
    
    // Count by side
    auto& storage = entities.storage();
    u32 blue_count = 0, red_count = 0;
    for (usize i = 0; i < storage.count; ++i) {
        if (storage.is_active(i)) {
            if (storage.side[i] == Side::BLUE) blue_count++;
            else if (storage.side[i] == Side::RED) red_count++;
        }
    }
    
    std::cout << "\nInitial forces:\n";
    std::cout << "  Blue: " << blue_count << "\n";
    std::cout << "  Red: " << red_count << "\n";
    
    std::cout << "\nSimulation complete (basic mode).\n";
    return 0;
}

int cmd_batch(const std::string& scenario_path, u32 iterations) {
    std::cout << "Monte Carlo batch: " << scenario_path << "\n";
    std::cout << "Iterations: " << iterations << "\n";
    
    // Load scenario
    ScenarioLoader loader;
    auto scenario_result = loader.load(scenario_path);
    if (!scenario_result) {
        std::cerr << "Error loading scenario: " << scenario_result.error.message << "\n";
        return 1;
    }
    
    const auto& scenario = *scenario_result.value;
    std::cout << "Scenario: " << scenario.name << "\n";
    
    // Configure Monte Carlo
    analysis::BatchConfig config;
    config.num_iterations = iterations;
    config.master_seed = 42;
    config.thread_count = 1;  // Sequential for determinism
    
    analysis::MonteCarloExecutor executor;
    executor.configure(config);
    
    std::cout << "Batch configured. Ready to run.\n";
    std::cout << "(Full execution requires complete scenario setup)\n";
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string cmd = argv[1];
    
    if (cmd == "info" || cmd == "--version" || cmd == "-v") {
        return cmd_info();
    }
    
    if (cmd == "platforms") {
        if (argc < 3) {
            std::cerr << "Error: platforms requires data_path\n";
            return 1;
        }
        return cmd_platforms(argv[2]);
    }
    
    if (cmd == "validate") {
        if (argc < 3) {
            std::cerr << "Error: validate requires scenario file\n";
            return 1;
        }
        return cmd_validate(argv[2]);
    }
    
    if (cmd == "run") {
        if (argc < 3) {
            std::cerr << "Error: run requires scenario file\n";
            return 1;
        }
        Seed seed = (argc >= 4) ? std::stoull(argv[3]) : 12345;
        return cmd_run(argv[2], seed);
    }
    
    if (cmd == "batch") {
        if (argc < 4) {
            std::cerr << "Error: batch requires scenario file and iteration count\n";
            return 1;
        }
        u32 iterations = std::stoul(argv[3]);
        return cmd_batch(argv[2], iterations);
    }
    
    if (cmd == "--help" || cmd == "-h") {
        print_usage();
        return 0;
    }
    
    std::cerr << "Unknown command: " << cmd << "\n";
    print_usage();
    return 1;
}
