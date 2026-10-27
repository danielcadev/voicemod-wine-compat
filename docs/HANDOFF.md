# Source snapshot handoff

Development and audio testing stopped after this authorized public source upload. No reminder, automation, additional installation, migration, or further experiment was scheduled. The working local research directory, private prefix, original application, and legacy path symlink were preserved separately.

This repository is a sanitized snapshot, not the active private runtime checkout. Personal filesystem paths in launch-voicemod.sh, compat/probe.c, and compat/session_probe.c were replaced with USERNAME. Before attempting a new setup, supply the official application and compatible toolchain, review path/endpoint assumptions, and read README.md. Do not infer that a fresh clone was integration-tested.

Verified software behavior and exact limits are in VERIFICATION.md. Clean Mic and Custom Pitch were tested. Other voices are unverified individually. The user reported successful audio after the local checks.

Possible work for a future user-requested session: review other available voices, improve configuration/portability, and evaluate the existing Hamilt79/linux-voicemod approach. No decision has been made to replace the working shim with that project, and no code comparison or runtime test of it has been performed.

Keep generated output, credentials, proprietary software, prefixes, account state, memory snapshots, and unreviewed logs outside Git. The ignore file uses explicit file allowlisting to help keep this snapshot restricted to reviewed text sources.
