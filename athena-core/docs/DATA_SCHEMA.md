# ATHENA - Data Schema

Versão do Schema: 1.1.2

## Scenario JSON

### Root Object

```json
{
  "id": "string (required)",
  "name": "string (required)",
  "version": "string (default: '1.0.0')",
  "metadata": { ... },
  "scale": "macro|meso|micro (default: meso)",
  "temporal": { ... },
  "spatial_bounds": { ... },
  "environment": { ... },
  "actors": [ ... ],
  "parameters": { ... }
}
```

### Metadata

```json
{
  "schema_version": "string (default: '1.0')",
  "created_at": "ISO-8601 datetime",
  "modified_at": "ISO-8601 datetime",
  "created_by": "string",
  "description": "string (optional)",
  "tags": ["string", ...]
}
```

### Temporal Config

```json
{
  "start_date": "ISO-8601 datetime",
  "end_date": "ISO-8601 datetime",
  "tick_duration_seconds": "number (default: 3600)",
  "max_ticks": "integer (0 = unlimited)"
}
```

### Spatial Bounds

```json
{
  "min_lat": "number (-90 to 90)",
  "max_lat": "number (-90 to 90)",
  "min_lon": "number (-180 to 180)",
  "max_lon": "number (-180 to 180)"
}
```

### Environment

```json
{
  "terrain": {
    "theater_id": "string",
    "resolution": "macro|meso|micro",
    "area_of_interest": { /* GeoBounds, optional */ }
  },
  "climate": {
    "season": "winter|spring|summer|fall",
    "weather_model": "string",
    "weather_effects": "boolean"
  },
  "infrastructure": {
    "roads": "boolean",
    "ports": "boolean",
    "airfields": "boolean",
    "bridges": "boolean",
    "critical_nodes": ["string", ...]
  }
}
```

### Actor Definition

```json
{
  "id": "string (required, unique)",
  "name": "string (default: id)",
  "type": "unit|formation|asset (default: unit)",
  "side": "blue|red|green|neutral (default: neutral)",
  
  "parent_id": "string (optional)",
  "subordinates": ["string", ...],
  
  "position": {
    "lat": "number",
    "lon": "number",
    "alt": "number (meters, default: 0)"
  },
  
  "mobility": {
    "max_speed_kmh": "number",
    "cruise_speed_kmh": "number",
    "terrain_factor": "number (0-1, default: 1.0)",
    "movement_type": "ground|air|naval|amphibious"
  },
  
  "firepower": {
    "base_firepower": "number",
    "range_km": "number",
    "accuracy": "number (0-1)",
    "rate_of_fire": "number (engagements per tick)"
  },
  
  "sensors": {
    "detection_range_km": "number",
    "identification_range_km": "number",
    "sensor_type": "radar|optical|sigint|combined"
  },
  
  "logistics": {
    "fuel_capacity": "number",
    "ammo_capacity": "number",
    "consumption_rate": "number (per tick)",
    "resupply_rate": "number (per tick at depot)"
  },
  
  "command": {
    "command_range_km": "number",
    "communication_reliability": "number (0-1)",
    "max_subordinates": "integer"
  },
  
  "health": "number (0-1, default: 1.0)",
  "supply": "number (0-1, default: 1.0)",
  "morale": "number (0-1, default: 1.0)",
  "readiness": "number (0-1, default: 1.0)",
  
  "behavior": "string (behavior script name)",
  "behavior_params": { "key": "number", ... }
}
```

### Uncertain Parameter

```json
{
  "parameter_name": {
    "value": "number (base value)",
    "uncertainty": {
      "type": "point|uniform|normal|lognormal|triangular|beta",
      
      // For point:
      "value": "number",
      
      // For uniform:
      "min": "number",
      "max": "number",
      
      // For normal:
      "mean": "number",
      "stddev": "number",
      
      // For lognormal:
      "mu": "number",
      "sigma": "number",
      
      // For triangular:
      "min": "number",
      "max": "number",
      "mode": "number",
      
      // For beta:
      "alpha": "number",
      "beta": "number"
    },
    "sensitivity": "boolean (include in sensitivity analysis)",
    "source": "string (data source)",
    "confidence": "number (0-1)"
  }
}
```

## Scale Definitions

| Scale | Resolution | Typical Use |
|-------|------------|-------------|
| macro | ~1km | Strategic, theater-level |
| meso | ~90m | Operational, campaign |
| micro | ~30m | Tactical, engagement |

## Side Values

| Value | Description |
|-------|-------------|
| blue | Friendly forces |
| red | Enemy forces |
| green | Allied/partner forces |
| neutral | Non-combatants |

## Entity Flags (Internal)

```cpp
namespace entity_flags {
    constexpr u8 ACTIVE    = 0x01;  // Entity is alive
    constexpr u8 DEAD      = 0x02;  // Entity destroyed
    constexpr u8 MOVING    = 0x04;  // Currently moving
    constexpr u8 ENGAGED   = 0x08;  // In combat this tick
    constexpr u8 DETECTED  = 0x10;  // Has been detected
    constexpr u8 SUPPLIED  = 0x20;  // At supply point
}
```

## Coordinate System

### Input (JSON)
- Latitude/Longitude (WGS84)
- Altitude in meters above sea level

### Internal (Simulation)
- Meters from scenario center
- Equirectangular projection
- Z = altitude in meters

### Conversion
```cpp
constexpr f64 EARTH_RADIUS = 6371000.0;  // meters
constexpr f64 DEG_TO_RAD = PI / 180.0;

// Center of scenario
f64 center_lat = (bounds.min_lat + bounds.max_lat) / 2.0;
f64 center_lon = (bounds.min_lon + bounds.max_lon) / 2.0;

// Convert point to meters
f64 x = EARTH_RADIUS * (lon - center_lon) * DEG_TO_RAD * cos(center_lat * DEG_TO_RAD);
f64 y = EARTH_RADIUS * (lat - center_lat) * DEG_TO_RAD;
f64 z = altitude;
```

## Terrain Configuration (NEW)

### Physical Layer

```json
{
  "terrain": {
    "physical": {
      "base": "flat|hills|mountains|valley|plateau",
      "roughness": "number (0-1, default: 0.5)",
      "seed": "integer (required, non-zero)",
      "base_elevation_m": "number (default: 100)",
      "elevation_range_m": "number (default: 200)",
      "width_m": "number (default: 100000)",
      "height_m": "number (default: 100000)",
      "overlays": [
        {
          "type": "forest|urban|swamp|river|lake|road|bridge",
          "density": "number (0-1)",
          "param1": "number (type-specific, e.g., river width)",
          "seed_offset": "integer (default: 0)"
        }
      ],
      "connectivity": {
        "north": "flat|hills|mountains|valley|plateau",
        "south": "flat|hills|mountains|valley|plateau",
        "east": "flat|hills|mountains|valley|plateau",
        "west": "flat|hills|mountains|valley|plateau"
      }
    }
  }
}
```

### Environmental Layer

```json
{
  "terrain": {
    "environment": {
      "climate": "temperate|arctic|tropical|arid|continental",
      "weather": "clear|overcast|rain|heavy_rain|snow|blizzard|fog|dust|sandstorm",
      "temperature_c": "number (Celsius)",
      "visibility_km": "number",
      "wind_speed_kmh": "number",
      "wind_direction_deg": "number (0-360, from)",
      "humidity_pct": "number (0-100)",
      "day_night_enabled": "boolean",
      "sunrise_hour": "number (0-24)",
      "sunset_hour": "number (0-24)",
      "current_hour": "number (0-24)"
    }
  }
}
```

### Events

```json
{
  "terrain": {
    "events": [
      {
        "type": "flood|fire|landslide|earthquake|chemical|nuclear",
        "center_x": "number (meters)",
        "center_y": "number (meters)",
        "radius_m": "number",
        "start_tick": "integer",
        "duration_ticks": "integer",
        "intensity": "number (0-1)",
        "spread_rate_m_per_tick": "number (default: 0)",
        "spread_direction_deg": "number (default: 0)"
      }
    ]
  }
}
```

### Terrain Enums

| Enum | Values |
|------|--------|
| TerrainBase | FLAT, HILLS, MOUNTAINS, VALLEY, PLATEAU |
| OverlayType | NONE, FOREST, URBAN, SWAMP, RIVER, LAKE, ROAD, BRIDGE |
| Climate | TEMPERATE, ARCTIC, TROPICAL, ARID, CONTINENTAL |
| Weather | CLEAR, OVERCAST, RAIN, HEAVY_RAIN, SNOW, BLIZZARD, FOG, DUST, SANDSTORM |
| EventType | NONE, FLOOD, FIRE, LANDSLIDE, EARTHQUAKE, CHEMICAL, NUCLEAR |
| MovementType | FOOT, WHEELED, TRACKED, AMPHIBIOUS, HELICOPTER, FIXED_WING, NAVAL |
| SensorType | VISUAL, THERMAL, RADAR, ACOUSTIC, SIGINT, SEISMIC |

### Semantic Layer (Derived - Not Stored)

The semantic layer is NEVER stored. It is computed on-demand from Physical + Environmental layers.

**Available queries:**
```cpp
// Movement
MovementCost movement_cost(x, y, tick, MovementType);
bool is_passable(x, y, tick, MovementType);

// Combat
DefenseValue defense_value(x, y, tick);  // {cover, concealment, advantage}
f64 cover(x, y, tick);
f64 concealment(x, y, tick);

// Sensors
SensorEffectiveness sensor_effectiveness(x, y, tick, SensorType);
f64 line_of_sight(x1, y1, x2, y2, tick);  // Returns 0-1 visibility fraction

// Operational
f64 armor_suitability(x, y, tick);
f64 infantry_suitability(x, y, tick);
f64 chokepoint_value(x, y);
```

## Monte Carlo Results (JSON Export)

```json
{
  "config": {
    "num_iterations": "integer",
    "master_seed": "integer"
  },
  "results": [
    {
      "iteration_id": "integer",
      "seed": "integer",
      "ticks_executed": "integer",
      "completed": "boolean",
      "blue_surviving": "integer",
      "red_surviving": "integer",
      "blue_total_health": "number",
      "red_total_health": "number"
    }
  ],
  "statistics": {
    "completed": "integer",
    "failed": "integer",
    "blue_wins": "integer",
    "red_wins": "integer",
    "draws": "integer"
  }
}
```

## Manifest Schema

```json
{
  "manifest_version": "1.0",
  "created_at": "ISO-8601",
  "job_id": "uuid",
  
  "platform": {
    "os_name": "string",
    "os_version": "string",
    "cpu_vendor": "string",
    "cpu_brand": "string",
    "cpu_microarch": "string",
    "cpu_features": "string",
    "cpu_cores": "integer",
    "cpu_threads": "integer",
    "ram_bytes": "integer"
  },
  
  "build": {
    "version": "string",
    "git_commit": "string",
    "build_date": "string",
    "compiler": "string",
    "compiler_flags": "string",
    "binary_hash": "string"
  },
  
  "config": {
    "master_seed": "integer",
    "rng_stream": "integer",
    "thread_count": "integer",
    "start_tick": "integer",
    "end_tick": "integer",
    "monte_carlo_iterations": "integer"
  },
  
  "input_hashes": {
    "scenario_hash": "sha256:...",
    "model_hash": "sha256:...",
    "parameters_hash": "sha256:..."
  },
  
  "results": {
    "ticks_executed": "integer",
    "entities_created": "integer",
    "entities_destroyed": "integer",
    "duration_seconds": "number",
    "completed_normally": "boolean",
    "error_message": "string"
  },
  
  "output_hash": "sha256:...",
  
  "chain": {
    "previous_manifest_hash": "sha256:...",
    "sequence_number": "integer"
  },
  
  "manifest_hash": "sha256:..."
}
```
