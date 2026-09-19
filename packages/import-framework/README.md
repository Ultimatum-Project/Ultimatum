# Ultimatum import framework

`src/installation-orchestrator.js` owns the generic prepare, stage, publish,
commit, rollback, cancellation, and progress lifecycle. A port adapter supplies
inventory, detection, validation, and its install plan; a library provider
publishes source data and the durable local-library record in one transaction.

The first adapter remains `clients/web/dist/import-adapter.js`. It accepts only
the reviewed Ultima IV profiles and never receives ambient filesystem access.
The browser build copies the shared orchestrator into its generated client; the
implementation is not duplicated in the port.

This extraction does not broaden accepted editions, move installed data, or
change `.u4save` packages. An interrupted or failed publication leaves both the
prior IndexedDB record and the engine's staged runtime files intact.
