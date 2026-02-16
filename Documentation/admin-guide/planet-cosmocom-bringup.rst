.. SPDX-License-Identifier: GPL-2.0-only

Planet Cosmo Communicator Bring-up (MT6771, Linux 6.4)
======================================================

This tree carries staged Cosmo-specific enablement for:

- touchscreen (Solomon SSD20xx, ``mediatek,solomon_touch``)
- keyboard and LEDs via AW9523 GPIO expanders
- USB-C controller (FUSB301)
- modem register/interrupt path scaffolding (CLDMA/CCIF)

Current status
--------------

The branch enables DT nodes and kernel support needed for initial probing.
The modem path is a scaffold driver for staged integration and debug, not a
complete cellular stack.

Quick runtime validation
------------------------

After booting the kernel on device:

.. code-block:: sh

   sudo mount -t debugfs none /sys/kernel/debug || true
   sudo sh scripts/cosmo-hw-selfcheck.sh

Non-destructive test boot (kexec)
---------------------------------

You can test the new kernel without flashing partitions:

.. code-block:: sh

   sudo apt-get install -y kexec-tools
   sudo sh scripts/test-kexec-cosmo.sh

The command above only loads the kernel. To jump into it:

.. code-block:: sh

   sudo systemctl kexec

Or one-shot load+switch:

.. code-block:: sh

   sudo sh scripts/test-kexec-cosmo.sh --exec

Expected debug output
---------------------

- modem scaffold debugfs state:

.. code-block:: sh

   ls /sys/kernel/debug/mtk-cosmo-modem
   cat /sys/kernel/debug/mtk-cosmo-modem/*/status

Notes
-----

- Camera and cellular data path support remain staged and require on-device
  validation and additional driver work beyond basic probe wiring.
