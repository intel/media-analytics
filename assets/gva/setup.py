#!/usr/bin/env python3

"""
DL Streamer Python Tools

DL Streamer is a streaming media analytics framework based on GStreamer*
multimedia framework, for creating complex media analytics pipelines.
It ensures pipeline interoperability and provides optimized media,
and inference operations using Intel OpenVINO Toolkit Inference Engine
backend.
"""

import pathlib
from setuptools import setup, find_packages


PYTHON_PROJECT = pathlib.Path(__file__).parent.resolve()

version = (PYTHON_PROJECT / "build" / "version.txt").read_text(encoding="utf-8").rstrip()
long_description = (PYTHON_PROJECT / "README.md").read_text(encoding="utf-8")


setup(
    name="gstgva",
    version=version,
    author="Dmitry Rogozhkin",
    author_email="dmitry.v.rogozhkin@intel.com",
    description="DL Streamer Python Tools",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/openvinotoolkit/dlstreamer_gst.git",
    package_dir={"": "python"},
    packages=[
        "gstgva",
        "gstgva.audio"
    ],
    install_requires=[
        "numpy>=1.19.2",
        "opencv-python>=4.2.0.34",
    ]
)
