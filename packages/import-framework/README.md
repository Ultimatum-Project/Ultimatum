# Ultimatum import framework

The first compatibility implementation is the active web adapter in
`clients/web/dist/import-adapter.js`. It inventories opaque entries, delegates
edition-specific validation to the Ultima IV port, creates an install plan, and
preserves the existing atomic publication and runtime rollback behavior.

This extraction does not broaden accepted editions or move installed data.
