# Project handoff

The project is presented as Voicemod for Linux. The GitHub repository URL remains voicemod-wine-compat. Public-facing copy uses plain language; implementation details are in the technical documentation. The original working research directory, private prefix, signed application files, and legacy path symlink were preserved separately.

At the user's request, v0.1.0 adds an easy-setup preview: install.sh downloads the matching setup.py, which installs checksum-pinned project components, configures a dedicated per-user prefix, runs the user-selected official installer, and creates application/sign-in shortcuts. Source probes now load their DLL relative to their own executable. No compiler is required for users of the release.

The local runtime audio evidence is in VERIFICATION.md. Clean Mic and Custom Pitch were tested. Other voices remain unverified individually. The user reported successful audio. Installer checks and remaining clean-install/login limits are in INSTALLATION.md. Do not describe the first full vendor-install/account journey or other distributions as validated.

The existing Hamilt79/linux-voicemod README was read; its implementation has not been evaluated and no migration to it was made. We acknowledge its related work and prebuilt Wine option.

Keep credentials, proprietary software, prefixes, account state, memory snapshots, and unreviewed logs outside Git and release assets. The release runtime archive contains only this project's six compiled components. Stop after completing this user-requested installer publication; future audio experiments or migrations need a new user request.
