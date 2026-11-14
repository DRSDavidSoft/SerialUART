# Embedded PortableShell

Source: https://github.com/atomicdeploy/portable-shell
Revision: 76685c29dc56842ea86e0a0e8dbf2cdb71b50c88 (0.14.0)
Imported: include/ and src/. MIT license is in LICENSE.
The centered panel dialog is backported from PortableShell PR #91
(https://github.com/atomicdeploy/portable-shell/pull/91), with the shared table
renderer update from PR #92 (https://github.com/atomicdeploy/portable-shell/pull/92).
Only the framework C core is compiled; workbench/host programs are not included.
SerialUART's native adapter is cli/tui.c. No package download or separately
installed PortableShell library is required at build time or runtime.
The host selects PS_EDITOR_CAPACITY=33792 consistently for framework and adapter.
