# Ultimatum diagnostics

This package defines the local diagnostics contract used by platform hosts and
ports. It provides bounded structured events, conservative field redaction and
support-bundle assembly without reading game data, save payloads, typed text,
input streams, screenshots or local paths.

`DiagnosticsCollector` is deliberately local-only. Creating a bundle does not
send it anywhere, and the existing feedback flow remains a separate explicit
user action. Ports declare stable diagnostic codes in a versioned manifest;
the platform owns collection and redaction.

The browser integration projects existing session, storage, settings and input
registries into this contract. It does not migrate or rewrite user data.
