# SkellyChoir 0.9.0 — Windows VST3 private preview

This package is an **unsigned testing preview**, not a commercial release. The installer does not change the registry, other plugins, presets, DAW projects, host state, or licensing files.

## Install

1. Close FL Studio and any other host that may have SkellyChoir loaded.
2. Extract the complete ZIP to a normal folder. Keep build-info.json, SHA256SUMS.txt, the installer, notices, and the SkellyChoir.vst3 folder together.
3. Open PowerShell 7 in that extracted folder.
4. Validate without changing files:

       .\Install-SkellyChoirPreview.ps1 -PackageRoot . -WhatIf

5. If validation passes, install with explicit confirmation:

       .\Install-SkellyChoirPreview.ps1 -PackageRoot .

The default install target is the per-user VST3 folder %LOCALAPPDATA%\Programs\Common\VST3; no administrator elevation is requested. If your host does not scan that folder, add it to the host's VST3 search paths or pass a different -Destination.

## Update and rollback

When a valid SkellyChoir installation already exists at the destination, the installer verifies its product identity and bundle hash, copies it to %LOCALAPPDATA%\Skelly\Backups\SkellyChoir, verifies that backup, stages the new bundle, and swaps it into place. An unknown or modified bundle is not overwritten.

Preview the rollback first:

       .\Install-SkellyChoirPreview.ps1 -Rollback -WhatIf

Then confirm it:

       .\Install-SkellyChoirPreview.ps1 -Rollback

Rollback restores only the immediately previous verified SkellyChoir bundle. The newer bundle is retained so the rollback can be reversed. A first-time install has no previous version to restore. If you used a custom -BackupRoot or -Destination, pass the same paths to rollback.

## Safety and acceptance

- -WhatIf validates the package and proposed operation without writing files.
- The package checksum manifest must cover every file other than SHA256SUMS.txt; missing, extra, duplicate, case-colliding, traversal, or tampered paths are rejected.
- The installer verifies preview provenance, pluginval strictness 5, strict VST3 metadata, the two preserved processor/controller class IDs, and an AMD64 PE32+ module.
- The installer is not code-signed and does not enable automatic updates.
- FL Studio scan/load, old-project compatibility, real musical A/B, owner listening approval, target-hardware CPU/latency, final attribution, and commercial licensing remain separate release gates.
- Do not redistribute this preview as a commercial release or treat a passing CI run as FL Studio acceptance.
- Owner-authorized 2026-11-25: validated Windows Actions testing artifacts are downloadable regardless of repository visibility. Other GitHub users may access public artifacts. Private vocal fixtures must remain outside the package.
