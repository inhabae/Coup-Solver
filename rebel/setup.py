from __future__ import annotations

from pathlib import Path

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup


ROOT = Path(__file__).resolve().parents[1]


extension = Pybind11Extension(
    "_small_coup_rebel",
    [
        str(ROOT / "rebel" / "small_coup_pybind.cpp"),
        str(ROOT / "coup" / "small_coup.cpp"),
        str(ROOT / "coup" / "small_coup_rebel.cpp"),
    ],
    include_dirs=[str(ROOT / "coup")],
    cxx_std=17,
    extra_compile_args=["-O2"],
)


setup(
    name="small-coup-rebel",
    version="0.1.0",
    ext_modules=[extension],
    cmdclass={"build_ext": build_ext},
)
