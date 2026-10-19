import argparse
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "host_render_tool", ROOT / "tools/render_operator_status.py"
)
renderer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(renderer)


class OfflineTests(unittest.TestCase):
    def test_offline_and_prepare_flags_cannot_trigger_network(self):
        with (
            mock.patch("sys.argv", ["renderer", "--offline", "--prepare-deps"]),
            mock.patch.object(
                renderer, "prepare_deps", side_effect=AssertionError("network path")
            ),
        ):
            with self.assertRaises(SystemExit) as error:
                renderer.main()
        self.assertEqual(error.exception.code, 2)

    def test_offline_missing_lvgl_never_prepares_dependencies(self):
        parser = renderer.build_arg_parser()
        args = parser.parse_args(["--offline"])
        with (
            mock.patch.object(renderer, "find_lvgl_dir", return_value=None),
            mock.patch.object(
                renderer, "prepare_deps", side_effect=AssertionError("network path")
            ),
        ):
            with self.assertRaises(SystemExit) as error:
                renderer.render_operator(args, parser)
        self.assertEqual(error.exception.code, 2)

    def test_registry_is_forwarded_to_host_build_not_firmware_config(self):
        parser = renderer.build_arg_parser()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            registry = root / "pending.h"
            registry.write_text("#define OPERATOR_THEME_REGISTRY_VERSION 1\n")
            args = parser.parse_args(
                [
                    "--offline",
                    "--registry",
                    str(registry),
                    "--cache-dir",
                    str(root),
                    "--output",
                    str(root / "out.png"),
                ]
            )
            commands = []
            with (
                mock.patch.object(renderer, "find_lvgl_dir", return_value=root),
                mock.patch.object(
                    renderer,
                    "run",
                    side_effect=lambda command, **kwargs: commands.append(command),
                ),
                mock.patch.object(renderer, "raw_to_png"),
            ):
                renderer.render_operator(args, parser)
            self.assertTrue(
                any(f"-DOPERATOR_REGISTRY={registry}" in c for c in commands)
            )


if __name__ == "__main__":
    unittest.main()
