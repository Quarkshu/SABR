from __future__ import annotations

import sys
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
        self.assertIn("coord::failures[0].probability", export_matrix_step["command"])
        self.assertIn("coord::redundancy.mode", export_matrix_step["command"])

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