# Old and new paths

Where each path of the four original repositories is in 51degrees-cxx. A folder
not listed moved with its parent. Deleted: each repository's `ci/` and
`.github/`, device-detection-cxx `CodeBlocks/`, common-cxx
`nightly-publish-main.yml` and `.gitmodules`, and the duplicate `LICENSE` and
`.gitignore` files.

| Old path | New path |
| --- | --- |
| `common-cxx/` | `src/common/` |
| `common-cxx/VisualStudio/` | `VisualStudio/Common/` |
| `common-cxx/cmake/` | `cmake/` |
| `common-cxx/performance/` | `test/common/performance/` |
| `common-cxx/tests/` | `test/common/` |
| `device-detection-cxx/` | `src/dd/` |
| `device-detection-cxx/VisualStudio/` | `VisualStudio/DeviceDetection/` |
| `device-detection-cxx/docs/` | `docs/dd/` |
| `device-detection-cxx/examples/C/Hash/` | `examples/dd/C/Hash/` |
| `device-detection-cxx/examples/CPP/Hash/` | `examples/dd/CPP/Hash/` |
| `device-detection-cxx/scripts/` | `scripts/` |
| `device-detection-cxx/src/` | `src/dd/` |
| `device-detection-cxx/test/` | `test/dd/` |
| `ip-graph-cxx/` | `src/ipi/graph/` |
| `ip-intelligence-cxx/` | `src/ipi/` |
| `ip-intelligence-cxx/VisualStudio/` | `VisualStudio/IpIntelligence/` |
| `ip-intelligence-cxx/docs/` | `docs/ipi/` |
| `ip-intelligence-cxx/examples/Base/` | `examples/ipi/Base/` |
| `ip-intelligence-cxx/examples/C/IpIntelligence/` | `examples/ipi/C/IpIntelligence/` |
| `ip-intelligence-cxx/examples/CPP/IpIntelligence/` | `examples/ipi/CPP/IpIntelligence/` |
| `ip-intelligence-cxx/src/` | `src/ipi/` |
| `ip-intelligence-cxx/test/` | `test/ipi/` |
| `common-cxx/.gitignore` | `.gitignore` |
| `common-cxx/LICENSE` | `LICENSE` |
| `device-detection-cxx/CMakeLists.txt` | `CMakeLists.txt` |
| `device-detection-cxx/README.md` | `docs/dd/README.md` |
| `ip-intelligence-cxx/CMakeLists.txt` | `CMakeLists.txt` |
| `ip-intelligence-cxx/Directory.Build.props` | `VisualStudio/IpIntelligence/Directory.Build.props` |
| `ip-intelligence-cxx/README.md` | `docs/ipi/README.md` |
| `ip-intelligence-cxx/STORED_WEIGHTED_VALUE_DESIGN_PROPOSAL.md` | `docs/ipi/STORED_WEIGHTED_VALUE_DESIGN_PROPOSAL.md` |
| `device-detection-cxx/device-detection-data` (submodule) | `device-detection-data` (submodule) |
| `ip-intelligence-cxx/ip-intelligence-data` (submodule) | `ip-intelligence-data` (submodule) |
| `device-detection-cxx/src/common-cxx/` (submodule) | the `common-cxx` rows above |
| `ip-intelligence-cxx/src/common-cxx/` (submodule) | the `common-cxx` rows above |
| `ip-intelligence-cxx/src/ip-graph-cxx/` (submodule) | the `ip-graph-cxx` rows above |

## In a language repository

A language repository mounts this repository as one submodule at `cxx/` in its
root.

- A native build includes `cxx/CMakeLists.txt`. `src/dd/CMakeLists.txt` and
  `src/ipi/CMakeLists.txt` only work when the root includes them. To leave out
  the product a package does not use, set `FIFTYONE_BUILD_DEVICE_DETECTION` or
  `FIFTYONE_BUILD_IP_INTELLIGENCE` to `OFF`.
- The SWIG interface files include `cxx/src/...`, so a generated wrapper needs
  the folder that holds `cxx/` on its include path. A wrapper already committed
  includes the old path, so it needs the same change or generating again.
