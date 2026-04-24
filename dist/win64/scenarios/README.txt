ATHENA Scenarios Directory

Save and load scenarios from this directory.
If you have scenarios saved with v1.1.2 or earlier, they may fail to load
due to missing temporal configuration. To fix, open the JSON and ensure:
  "temporal": { "tick_duration_seconds": 3600.0, "max_ticks": 72 }
