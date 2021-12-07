Generating Dockerfiles
======================

.. contents::

Overview
--------

Project includes pre-generated dockerfiles in the `docker <../docker>`_
folder for the key possible setups. If you've done any customizations to the
dockefiles template sources, regenerate dockerfiles with the following
commands::

  cmake .
  make

Dockerfiles Templates Structure
-------------------------------

Top level Dockerfile templates are located in the subfolders of
`docker <../docker>`_ folder:

* `docker/va-samples/ubuntu20.04/intel-gfx/Dockerfile.m4 <../docker/va-samples/ubuntu20.04/intel-gfx/Dockerfile.m4>`_
* `docker/gst-gva/ubuntu20.04/intel-gfx/Dockerfile.m4 <../docker/gst-gva/ubuntu20.04/intel-gfx/Dockerfile.m4>`_

These templates include component ingredients defined in the .m4 files
stored in `templates <../templates>`_ folder.

Templates Parameters
--------------------

It is possible to customize dockerfile setup passing some parameters during
Dockerfile generation from templates.

DEVEL
  Possible values: ``yes|no``. Default value: ``yes``

  Switches on/off development build type with which container user is
  created with sudo privileges.

