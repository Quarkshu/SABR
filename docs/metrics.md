# Metrics Reference

The primary machine-readable result contract is `stats.json`, plus the `bundle_summary.csv`, `contact_utilization.csv`, and aggregated summary CSV outputs.

## stats.json

Top-level sections:

- `metadata`
- `summary`
- `bundles`
- `contacts`
- `routing`
- `plan_updates`

## metadata

| Field | Meaning |
|---|---|
| `scenario_name` | Resolved scenario name written by the engine or experiment runner |
| `start_time` | Simulation start time |
| `end_time` | Simulation end time |
| `failure_seed` | Failure injection seed used for deterministic replay |
| `contact_plan_version` | Final contact plan version visible to the simulation |

## summary

| Field | Meaning |
|---|---|
| `bundles_created` | Number of generated bundles |
| `bundles_forwarded` | Number of forwarding events |
| `bundles_delivered` | Number of delivered bundles |
| `bundles_expired` | Number of expired bundles |
| `route_failures` | Number of route failure terminal outcomes |
| `tx_started` | Number of transmission start events |
| `tx_completed` | Number of transmission completion events |
| `contact_start_events` | Contact opening events processed |
| `contact_end_events` | Contact closing events processed |
| `routing_invocations` | Total routing attempts |
| `reroute_invocations` | Reroute attempts |
| `plan_update_events` | Dynamic plan update events |
| `queued_bundle_replans` | Queued bundles re-evaluated after a plan change |
| `stale_contact_events_ignored` | Ignored stale contact events |
| `reactive_anti_loop_trigger_count` | Reactive anti-loop intervention count |
| `local_repair_count` | Local repair execution count |
| `redundancy_trigger_count` | Redundancy expansion count |
| `redundant_first_hit_count` | First-hit wins successes from redundant families |
| `duplicate_replica_discard_count` | Duplicate replica drops |
| `redundancy_benefit_cost_ratio` | Benefit/cost estimate for redundancy mode |
| `delivery_rate` | Delivered bundles divided by created bundles |
| `average_delivery_latency` | Average end-to-end delay among delivered bundles |
| `max_delivery_latency` | Maximum end-to-end delay among delivered bundles |
| `average_hop_count` | Average hop count among delivered bundles |

## bundle_summary.csv

Columns:

- `bundle_id`
- `source_node`
- `destination_node`
- `final_state`
- `creation_time`
- `expiration_time`
- `delivered_time`
- `end_to_end_delay`
- `hop_count`
- `route_attempts`
- `is_critical`
- `is_multicast`
- `route_path`

## contact_utilization.csv

Columns:

- `contact_id`
- `plan_version`
- `sending_node`
- `receiving_node`
- `start_time`
- `end_time`
- `data_rate`
- `capacity`
- `committed_volume`
- `commit_count`
- `tx_started`
- `tx_completed`
- `utilization_ratio`

## Aggregated Exports

`aggregated_summary.csv` contains one row per run. It preserves:

- manifest fields such as `run_id`, `scenario_name`, `output_directory`
- matrix coordinate fields prefixed with `coord::`
- metadata fields prefixed with `metadata::`
- summary fields prefixed with `summary::`

`aggregated_pivot.csv` is generated when `export --format pivot` or `export --format both` is used. Its structure depends on:

- `--metric`
- `--row-key`
- `--column-key`

Recommended comparison metrics:

- `summary::delivery_rate`
- `summary::average_delivery_latency`
- `summary::average_hop_count`
- `summary::route_failures`
- `summary::local_repair_count`
- `summary::redundancy_trigger_count`

## performance_report.json

`scripts/benchmark_nf_pf_01.py` writes `performance_report.json` for the Stage 9 NF-PF-01 validation flow.

Top-level fields:

- `benchmark_name`
- `max_runtime_seconds`
- `workspace_root`
- `sabr_executable`
- `scenario_path`
- `output_root`
- `environment`
- `scenario`
- `threshold_satisfied_by_definition`
- `dry_run`
- `command`
- `wall_clock_seconds`
- `stdout`
- `stderr`
- `return_code`
- `generated_files`
- `summary`
- `metadata`
- `passed`
- `result`

Important Stage 9 benchmark fields:

| Field | Meaning |
|---|---|
| `scenario.node_count` | Number of nodes encoded into the generated benchmark scenario |
| `scenario.contact_count` | Number of contacts encoded into the generated benchmark scenario |
| `scenario.bundle_count` | Total generated bundle count across all benchmark traffic patterns |
| `max_runtime_seconds` | NF-PF-01 threshold, currently `60.0` seconds |
| `wall_clock_seconds` | Measured end-to-end benchmark runtime |
| `threshold_satisfied_by_definition` | Whether the generated scenario actually matches the 20-node / 200-contact / 1000-bundle requirement |
| `passed` | Whether the measured runtime stayed below the NF-PF-01 threshold |
| `result` | `prepared`, `passed`, `threshold_exceeded`, or `execution_failed` |

The embedded `summary` and `metadata` sections reuse the same `stats.json` field definitions documented above.