from __future__ import annotations

import csv
import json
from pathlib import Path


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(document: object, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2), encoding="utf-8")


def load_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def write_csv(rows: list[dict[str, object]], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames: list[str] = []
    seen: set[str] = set()
    for row in rows:
        for key in row.keys():
            if key not in seen:
                seen.add(key)
                fieldnames.append(key)

    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def flatten_manifest_and_stats(manifest_path: Path) -> dict[str, object]:
    manifest = load_json(manifest_path)
    stats_path = Path(manifest.get("generated_files", {}).get("stats", ""))
    row: dict[str, object] = {
        "run_id": manifest.get("run_id", ""),
        "experiment_name": manifest.get("experiment_name", ""),
        "scenario_name": manifest.get("scenario_name", ""),
        "scenario_file": manifest.get("scenario_file", ""),
        "output_directory": manifest.get("output_directory", ""),
        "success": manifest.get("success", False),
        "error_message": manifest.get("error_message", ""),
    }

    for coordinate in manifest.get("matrix_coordinates", []):
        row[f"coord::{coordinate['name']}"] = coordinate.get("value")

    if stats_path.exists():
        stats = load_json(stats_path)
        metadata = stats.get("metadata", {})
        summary = stats.get("summary", {})
        for key, value in metadata.items():
            row[f"metadata::{key}"] = value
        for key, value in summary.items():
            row[f"summary::{key}"] = value

    return row


def discover_manifest_paths(input_path: Path) -> list[Path]:
    resolved = input_path.resolve()
    if resolved.is_file():
        if resolved.name == "run_manifest.json":
            return [resolved]
        return []
    return sorted(resolved.rglob("run_manifest.json"))


def collect_flat_rows(input_path: Path) -> list[dict[str, object]]:
    resolved = input_path.resolve()
    if resolved.is_file() and resolved.suffix.lower() == ".csv":
        return [dict(row) for row in load_csv(resolved)]
    if resolved.is_file() and resolved.name == "run_manifest.json":
        return [flatten_manifest_and_stats(resolved)]

    manifest_paths = discover_manifest_paths(resolved)
    return [flatten_manifest_and_stats(path) for path in manifest_paths]


def parse_numeric(value: object) -> float | None:
    if isinstance(value, bool):
        return 1.0 if value else 0.0
    if isinstance(value, (int, float)):
        return float(value)
    if value in (None, ""):
        return None
    try:
        return float(str(value))
    except (TypeError, ValueError):
        return None


def average_numeric(values: list[object]) -> float | str:
    numerics = [parsed for value in values if (parsed := parse_numeric(value)) is not None]
    if not numerics:
        return ""
    return sum(numerics) / len(numerics)


def build_pivot_rows(rows: list[dict[str, object]],
                     row_key: str,
                     column_key: str,
                     metric: str) -> list[dict[str, object]]:
    if not rows:
        return []

    row_values = sorted({str(row.get(row_key, "")) for row in rows})
    if not column_key:
        pivot_rows: list[dict[str, object]] = []
        for row_value in row_values:
            matched = [row for row in rows if str(row.get(row_key, "")) == row_value]
            pivot_rows.append({
                row_key: row_value,
                metric: average_numeric([row.get(metric) for row in matched]),
            })
        return pivot_rows

    column_values = sorted({str(row.get(column_key, "")) for row in rows})
    pivot_rows = []
    for row_value in row_values:
        pivot_row: dict[str, object] = {row_key: row_value}
        for column_value in column_values:
            matched = [row for row in rows
                       if str(row.get(row_key, "")) == row_value
                       and str(row.get(column_key, "")) == column_value]
            pivot_row[column_value] = average_numeric([row.get(metric) for row in matched])
        pivot_rows.append(pivot_row)
    return pivot_rows


def resolve_run_directory(input_path: Path) -> Path:
    resolved = input_path.resolve()
    if resolved.is_dir():
        return resolved
    if resolved.name in {"run_manifest.json", "stats.json", "bundle_summary.csv", "contact_utilization.csv"}:
        return resolved.parent
    raise FileNotFoundError(f"unable to resolve run directory from {resolved}")


def resolve_stats_path(input_path: Path) -> Path:
    resolved = input_path.resolve()
    if resolved.is_file() and resolved.name == "stats.json":
        return resolved
    run_dir = resolve_run_directory(resolved)
    stats_path = run_dir / "stats.json"
    if stats_path.exists():
        return stats_path
    if (run_dir / "run_manifest.json").exists():
        manifest = load_json(run_dir / "run_manifest.json")
        candidate = Path(manifest.get("generated_files", {}).get("stats", ""))
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"missing stats.json under {run_dir}")


def resolve_contact_utilization_csv_path(input_path: Path) -> Path:
    resolved = input_path.resolve()
    if resolved.is_file() and resolved.name == "contact_utilization.csv":
        return resolved
    run_dir = resolve_run_directory(resolved)
    csv_path = run_dir / "contact_utilization.csv"
    if not csv_path.exists():
        raise FileNotFoundError(f"missing contact_utilization.csv under {run_dir}")
    return csv_path


def load_stats(input_path: Path) -> dict:
    return load_json(resolve_stats_path(input_path))


def load_contact_utilization_rows(input_path: Path) -> list[dict[str, str]]:
    return load_csv(resolve_contact_utilization_csv_path(input_path))


def resolve_contact_plan_path(input_path: Path) -> Path:
    resolved = input_path.resolve()
    if resolved.suffix.lower() in {".ion", ".json"}:
        if resolved.suffix.lower() == ".json":
            document = load_json(resolved)
            if "contact_plan_file" in document:
                return (resolved.parent / document["contact_plan_file"]).resolve()
            if "contact_plan" in document or "contacts" in document:
                return resolved
        else:
            return resolved

    if resolved.is_dir() and (resolved / "run_manifest.json").exists():
        manifest = load_json(resolved / "run_manifest.json")
        scenario_path = Path(manifest.get("scenario_file", ""))
        if scenario_path.exists():
            return resolve_contact_plan_path(scenario_path)

    if resolved.name == "run_manifest.json":
        manifest = load_json(resolved)
        scenario_path = Path(manifest.get("scenario_file", ""))
        if scenario_path.exists():
            return resolve_contact_plan_path(scenario_path)

    raise FileNotFoundError(f"unable to resolve contact plan from {resolved}")


def parse_ion_contact_plan(path: Path) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    contacts: list[dict[str, object]] = []
    ranges: list[dict[str, object]] = []
    next_contact_id = 1
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line or line.startswith("//"):
            continue
        parts = line.split()
        if len(parts) < 2 or parts[0] != "a":
            continue
        if parts[1] == "contact" and len(parts) >= 7:
            contacts.append({
                "contact_id": next_contact_id,
                "start_time": float(parts[2].lstrip("+")),
                "end_time": float(parts[3].lstrip("+")),
                "sending_node": int(parts[4]),
                "receiving_node": int(parts[5]),
                "data_rate": float(parts[6]),
            })
            next_contact_id += 1
        elif parts[1] == "range" and len(parts) >= 7:
            ranges.append({
                "start_time": float(parts[2].lstrip("+")),
                "end_time": float(parts[3].lstrip("+")),
                "node_a": int(parts[4]),
                "node_b": int(parts[5]),
                "distance": float(parts[6]),
            })
    return contacts, ranges


def parse_json_contact_plan(path: Path) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    document = load_json(path)
    root = document.get("contact_plan", document)
    return list(root.get("contacts", [])), list(root.get("ranges", []))


def load_contact_plan(input_path: Path) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    contact_plan_path = resolve_contact_plan_path(input_path)
    if contact_plan_path.suffix.lower() == ".ion":
        return parse_ion_contact_plan(contact_plan_path)
    return parse_json_contact_plan(contact_plan_path)