Info and Statistics
===================

.. contents::

Overview
--------

Docker images include some tools to query basic system information relevant
to the key installed software and suggested workloads. This articles gives
instruction on how to use these tools.

We would like to pay attention on the following tools:

info
  See: `info <../assets/info>`_

  This is a small script which prints key system information. Provide this
  info to help triage issues you've met.

vainfo
  This is a tool which reports media driver capabilities.

clinfo
  This is a tool which reports OpenCL driver capabilities.

perf
  This is a Linux perf tool which allows to get variety of HW and SW
  performance metrics.

intel_gpu_top
  This is a tool which reports Intel GPU utilization along with few other
  useful characteristics such as current GPU frequency and interrupts
  number.

Running tool in parallel to workload
------------------------------------

Usage of some tools is clear - you just enter container and execute them.
This would go for ``info``, ``vainfo``, ``clinfo`` and Linux ``perf`` (till
you use ``perf`` as a proxy command to actual workload). We still will give
some advices on particular command lines to use. At the moment, however,
let's focus on another type of usage specific to ``intel_gpu_top`` and Linux
``perf`` - when these tools are being executed in parallel to the running
workload. Container specifics here is that you need to get additional
console to actually run these tool in parrallel to your application.

The advice here is to use ``--name <name>`` docker-run option, i.e. assign
container a name to ease accessing container 2nd time from another shell::

  DEVICE=${DEVICE:-/dev/dri/renderD128}
  DEVICE_GRP=$(ls -g $DEVICE | awk '{print $3}' | \
    xargs getent group | awk -F: '{print $3}')
  if [ -e /dev/dri/by-path ]; then BY_PATH="-v /dev/dri/by-path:/dev/dri/by-path"; fi
  docker run --rm -it \
    -e DEVICE=$DEVICE --device $DEVICE --group-add $DEVICE_GRP $BY_PATH \
    --cap-add SYS_ADMIN \
    --name ma \
    intel-media-analytics

So, assuming you've entered container with the docker-run command above and
started some workload, you now can add ``intel_gpu_top`` running in the same
container in parallel to your workload::

  docker exec ma /bin/demo-bash intel_gpu_top -d drm:$DEVICE

Mind explicit specification if ``demo-bash`` entrypoint script. It is
needed instead of generic ``bash`` to fetch container environment, i.e.
location of apps installed in the container.

If you just need to enter container, run::

  docker exec ma /bin/hello-bash

Tips on vainfo
--------------

You can specify which backend and which device to use as follows::

  vainfo --display drm --device $DEVICE

Also pay attention on ``-a`` option which gives detailed driver
capabilities::

  vainfo --display drm --device $DEVICE -a

Tips on intel_gpu_top
---------------------

Use ``-d`` option to specify device you want to monitor::

  intel_gpu_top -d drm:$DEVICE

Pay attention on option ``-L`` which lists all available GPU devices::

  intel_gpu_top -L

You may find useful to use option ``-l`` to dump data in rough text format.
