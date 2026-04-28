from __future__ import annotations

import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import benchmark_nf_pf_01  # noqa: E402
import stage9_suite  # noqa: E402


class BenchmarkHarnessTest(unittest.TestCase):
    def test_benchmark_scenario_matches_nf_pf_01_shape(self) -> None:
        document = benchmark_nf_pf_01.build_scenario_document()
        summary = benchmark_nf_pf_01.summarize_scenario(document)

        self.assertEqual(summary["node_count"], benchmark_nf_pf_01.NODE_COUNT)
        self.assertEqual(summary["contact_count"], benchmark_nf_pf_01.CONTACT_COUNT)
        self.assertEqual(summary["bundle_count"], benchmark_nf_pf_01.TOTAL_BUNDLES)
        self.assertGreaterEqual(summary["range_count"], 20)


class Stage9SuitePlanTest(unittest.TestCase):
    def test_stage9_plan_covers_all_experiment_families(self) -> None:
        plan = stage9_suite.build_execution_plan(
            REPO_ROOT,
            REPO_ROOT / "build" / "sabr.exe",
            REPO_ROOT / "results" / "stage9_test_plan",
            9,
            set(),
            True,
            True,
        )

        experiment_names = [entry["experiment_name"] for entry in plan["experiments"]]
        self.assertEqual(
            experiment_names,
            [
                "exp1_unicast_correctness",
                "exp2_unicast_scale",
                "exp3_multicast_plan",
                "exp4_multicast_repair",
                "exp5_redundancy",
                "exp6_ablation",
            ],
        )
        self.assertIsNotNone(plan["benchmark"])
        self.assertIsNotNone(plan["bundle"])

    def test_release_bundle_sources_include_standard_delivery_set(self) -> None:
        sources = stage9_suite.build_release_sources(REPO_ROOT, REPO_ROOT / "results" / "stage9_test_bundle")
        destinations = {entry["destination"] for entry in sources}

        self.assertIn("README.md", destinations)
        self.assertIn("docs", destinations)
        self.assertIn("configs/experiments", destinations)
        self.assertIn("scripts/benchmark_nf_pf_01.py", destinations)
        self.assertIn("scripts/stage9_suite.py", destinations)
        self.assertIn("samples/performance", destinations)


if __name__ == "__main__":
    unittest.main()