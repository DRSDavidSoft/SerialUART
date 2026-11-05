# Embedded PortableShell

Source: https://github.com/atomicdeploy/portable-shell
Revision: 76685c29dc56842ea86e0a0e8dbf2cdb71b50c88 (0.14.0)
Imported: include/ and src/, unmodified. MIT license is in LICENSE.
Only the framework C core is compiled; workbench/host programs are not included.
SerialUART's native adapter is cli/tui.c. No package download or separately
installed PortableShell library is required at build time or runtime.
The host selects PS_EDITOR_CAPACITY=16384 consistently for framework and adapter.
