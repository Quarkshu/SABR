from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import complete_experiment_workflow as workflow  # noqa: E402
import run_exp1_complete  # noqa: E402
import run_exp2_complete  # noqa: E402
import run_exp3_complete  # noqa: E402
import run_exp4_complete  # noqa: E402
import run_exp5_complete  # noqa: E402
import run_exp6_complete  # noqa: E402
import sabr_results  # noqa: E402


class CompleteExperimentWorkflowTest(unittest.TestCase):
    def test_catalog_covers_all_complete_experiment_scripts(self) -> None:
        catalog = workflow.build_complete_experiment_catalog(REPO_ROOT)

        self.assertEqual(
            list(catalog.keys()),
            [
                "exp1_unicast_correctness",
                "exp2_unicast_scale",
                "exp3_multicast_plan",
                "exp4_multicast_repair",
                "exp5_redundancy",
                "exp6_ablation",
            ],
        )

    def test_exp1_plan_includes_three_scenarios_and_matrix(self) -> None:
        plan = workflow.build_execution_plan(
            REPO_ROOT,
            REPO_ROOT / "build" / "sabr.exe",
            "exp1_unicast_correctness",
            REPO_ROOT / "results" / "exp1_complete_test_plan",
        )

        step_names = [step["name"] for step in plan["steps"]]
        self.assertEqual(
            step_names,
            [
                "baseline",
                "reference_baseline",
                "downlink_split",
                "matrix",
                "export_scenarios",
                "export_matrix",
                "topology",
                "baseline_timeline",
                "baseline_utilization",
                "scenarios_comparison",
                "matrix_comparison",
            ],
        )

    def test_exp5_plan_includes_all_baselines_and_matrix_export(self) -> None:
        plan = workflow.build_execution_plan(
            REPO_ROOT,
            REPO_ROOT / "build" / "sabr.exe",
            "exp5_redundancy",
            REPO_ROOT / "results" / "exp5_complete_test_plan",
        )

        step_names = [step["name"] for step in plan["steps"]]
        self.assertEqual(step_names[:5], [
            "primary_only",
            "single_backup",
            "multi_backup",
            "trunk_only",
            "matrix",
        ])
        export_matrix_step = next(step for step in plan["steps"] if step["name"] == "export_matrix")
        export_matrix_primary_only_step = next(step for step in plan["steps"] if step["name"] == "export_matrix_primary_only")
        export_matrix_single_backup_step = next(step for step in plan["steps"] if step["name"] == "export_matrix_single_backup")
        matrix_primary_only_plot_step = next(step for step in plan["steps"] if step["name"] == "matrix_primary_only_comparison")
        matrix_single_backup_plot_step = next(step for step in plan["steps"] if step["name"] == "matrix_single_backup_comparison")
        self.assertIn("coord::failures[0].probability", export_matrix_step["command"])
        self.assertIn("coord::redundancy.mode", export_matrix_step["command"])
        self.assertIn("exp5_redundancy_primary_only", export_matrix_primary_only_step["input_path"])
        self.assertIn("exp5_redundancy_single_backup", export_matrix_single_backup_step["input_path"])
        self.assertIn("matrix_primary_only", export_matrix_primary_only_step["output_path"])
        self.assertIn("matrix_single_backup", export_matrix_single_backup_step["output_path"])
        self.assertIn("exp5_matrix_primary_only_comparison.svg", matrix_primary_only_plot_step["output_path"])
        self.assertIn("exp5_matrix_single_backup_comparison.svg", matrix_single_backup_plot_step["output_path"])

    def test_exp3_plan_uses_wildcard_matrix_coordinates(self) -> None:
        plan = workflow.build_execution_plan(
            REPO_ROOT,
            REPO_ROOT / "build" / "sabr.exe",
            "exp3_multicast_plan",
            REPO_ROOT / "results" / "exp3_complete_test_plan",
        )

        export_matrix_step = next(step for step in plan["steps"] if step["name"] == "export_matrix")
        matrix_plot_step = next(step for step in plan["steps"] if step["name"] == "matrix_comparison")

        self.assertIn("coord::traffic[*].bundle_count", export_matrix_step["command"])
        self.assertNotIn("coord::traffic[0].bundle_count", export_matrix_step["command"])
        self.assertIn("coord::traffic[*].bundle_count", matrix_plot_step["command"])
        self.assertNotIn("coord::traffic[0].bundle_count", matrix_plot_step["command"])

    def test_exp4_plan_uses_receiver_completion_metric(self) -> None:
        plan = workflow.build_execution_plan(
            REPO_ROOT,
            REPO_ROOT / "build" / "sabr.exe",
            "exp4_multicast_repair",
            REPO_ROOT / "results" / "exp4_complete_test_plan",
        )

        export_matrix_step = next(step for step in plan["steps"] if step["name"] == "export_matrix")
        matrix_plot_step = next(step for step in plan["steps"] if step["name"] == "matrix_comparison")

        self.assertIn("derived::receiver_completion", export_matrix_step["command"])
        self.assertNotIn("summary::delivery_rate", export_matrix_step["command"])
        self.assertIn("derived::receiver_completion", matrix_plot_step["command"])
        self.assertNotIn("summary::delivery_rate", matrix_plot_step["command"])

    def test_flatten_manifest_adds_multicast_receiver_completion_metric(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir_name:
            temp_dir = Path(temp_dir_name)
            scenario_path = temp_dir / "scenario.json"
            stats_path = temp_dir / "stats.json"
            manifest_path = temp_dir / "run_manifest.json"

            workflow.write_json(
                {
                    "scenario_name": "exp4_multicast_repair_baseline",
                    "multicast_groups": [
                        {
                            "group_id": "ops_broadcast",
                            "source_node": 1,
                            "member_nodes": [4, 5],
                        }
                    ],
                    "traffic": [
                        {
                            "mode": "BATCH",
                            "source_node": 1,
                            "multicast_group_id": "ops_broadcast",
                            "is_multicast": True,
                            "bundle_count": 4,
                        }
                    ],
                },
                scenario_path,
            )
            workflow.write_json(
                {
                    "metadata": {"scenario_name": "exp4_multicast_repair_baseline"},
                    "summary": {
                        "bundles_created": 4,
                        "bundles_delivered": 6,
                    },
                },
                stats_path,
            )
            workflow.write_json(
                {
                    "run_id": "matrix_0001",
                    "experiment_name": "exp4_multicast_repair",
                    "scenario_name": "exp4_multicast_repair_baseline__matrix_0001",
                    "scenario_file": str(scenario_path),
                    "output_directory": str(temp_dir),
                    "success": True,
                    "error_message": "",
                    "matrix_coordinates": [],
                    "generated_files": {
                        "stats": str(stats_path),
                    },
                },
                manifest_path,
            )

            row = sabr_results.flatten_manifest_and_stats(manifest_path)

            self.assertEqual(row["derived::multicast_group_size"], 2)
            self.assertAlmostEqual(row["derived::receiver_completion"], 0.75)

    def test_wrapper_scripts_map_to_unique_experiments(self) -> None:
        self.assertEqual(
            [
                run_exp1_complete.EXPERIMENT_NAME,
                run_exp2_complete.EXPERIMENT_NAME,
                run_exp3_complete.EXPERIMENT_NAME,
                run_exp4_complete.EXPERIMENT_NAME,
                run_exp5_complete.EXPERIMENT_NAME,
                run_exp6_complete.EXPERIMENT_NAME,
            ],
            [
                "exp1_unicast_correctness",
                "exp2_unicast_scale",
                "exp3_multicast_plan",
                "exp4_multicast_repair",
                "exp5_redundancy",
                "exp6_ablation",
            ],
        )


if __name__ == "__main__":
    unittest.main()