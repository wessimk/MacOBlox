"""No-auth timer regression with map-only negative controls in own prefixes."""
import argparse
from contextlib import contextmanager
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "launcher"))
from macoblox import darling_patches


@contextmanager
def fixture_directory():
    directory = Path(tempfile.mkdtemp(prefix="macoblox-timer-regression-"))
    try:
        yield directory
    finally:
        # Darling's stopped overlay workdir may remain root-owned.
        shutil.rmtree(directory, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sysroot", type=Path, default=Path("/usr/libexec/darling"))
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "logs/darling-timers")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with fixture_directory() as directory:
        directory = Path(directory)
        binary = directory / "timer-test"
        subprocess.run(["clang", "-target", "x86_64-apple-darwin", "-fuse-ld=lld",
                        "-isysroot", str(args.sysroot), "-mmacosx-version-min=11.0",
                        str(ROOT / "tests/darling_timer_test.c"), "-o", str(binary)], check=True)
        for repaired in (False, True):
            prefix = directory / ("repaired" if repaired else "map-only")
            env = dict(os.environ, DPREFIX=str(prefix))
            for key in ("DYLD_INSERT_LIBRARIES", "DYLD_FORCE_FLAT_NAMESPACE", "LD_PRELOAD"):
                env.pop(key, None)
            try:
                subprocess.run(["darling", "shell", "true"], env=env, check=True,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
                subprocess.run(["darling", "shutdown"], env=env, check=True, timeout=15)
                releases = (darling_patches.SPARSE_SYSTEM_RELEASE, darling_patches.SPARSE_LIBC_RELEASE)
                for release in releases:
                    source = args.sysroot / release.library
                    status, data = darling_patches.plan_sparse_map_patch(source.read_bytes(), releases,
                                                                        library=release.library)
                    if status != "patched":
                        raise RuntimeError(f"Unsupported runtime for negative control: {release.library}: {status}")
                    target = prefix / release.library
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
                    target.chmod(source.stat().st_mode & 0o777)
                    if repaired:
                        status = darling_patches.install_sparse_map_copy(source, target, library=release.library)
                        if status != "patched":
                            raise RuntimeError(f"Runtime migration failed: {status}")
                for mode in ("raw", "dispatch"):
                    for shim in ((False, True) if repaired else (False,)):
                        command = ["darling", "shell", "/usr/bin/env"]
                        if shim:
                            command += ["DYLD_FORCE_FLAT_NAMESPACE=1",
                                        "DYLD_INSERT_LIBRARIES=/Volumes/SystemRoot" + str(args.build_dir.resolve() / "libMacOBloxShims.dylib")]
                        command += ["/Volumes/SystemRoot" + str(binary), mode]
                        name = f"{'repaired' if repaired else 'map-only'}-{mode}-shim-{int(shim)}"
                        # Prefix daemons inherit output descriptors: a pipe's
                        # EOF can outlive the frontend. Use a real log file.
                        with (args.output_dir / f"{name}.log").open("w") as log:
                            result = subprocess.run(command, env=env, stdout=log,
                                                    stderr=subprocess.STDOUT, timeout=30)
                        expected = 0 if repaired else 3
                        if result.returncode != expected:
                            raise RuntimeError(f"{name}: expected {expected}, got {result.returncode}; see log")
                        print(f"PASS {name}: exit={result.returncode}", flush=True)
            finally:
                subprocess.run(["darling", "shutdown"], env=env, timeout=15, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
