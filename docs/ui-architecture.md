# UI Architecture Decisions

## Direction

- Build the user interface as a browser-based application.
- Use the same frontend for browser access and the desktop application, embedding it in a desktop webview rather than maintaining separate native and web UIs.
- Use a REST/JSON API between the UI and the C++ server. Keep receipt processing and database operations behind transport-independent server logic so both browser and desktop deployments use the same API.
- The REST server serves the Vue build and implements the item search/history, receipt review/confirmation, and uploaded batch-processing endpoints.
- The desktop Qt application is a thin Qt WebEngine shell that opens the existing server; the server must be started separately. Its URL is configurable with `--url`.

## Frontend stack

- **Vue 3**, **TypeScript**, and **Vite**.
- Use responsive layouts and standard browser APIs to support Brave and Safari on iOS, and Brave, Chrome, and Firefox on Ubuntu and Windows laptops.
- Use an OpenAPI document as the REST API contract. Generate TypeScript types or a client from it where practical, and keep the server implementation and frontend aligned with the contract.
- **Chart.js** renders item price history.
- **Qt WebEngine** embeds the shared frontend in the desktop application.

## API considerations

- Define endpoints and response schemas that cover the current UI needs: item search and history, item-code lookup, image processing, and confirmation of processed items.
- Upload receipt images as files (for example, multipart form data) rather than base64-encoding them in JSON.
- For long-running batch processing, return a job identifier and provide progress through polling.
- The OpenAPI schema generates TypeScript definitions for the frontend; multipart uploads use `file` for review and repeated `files` fields for batches.
- The selected initial deployment allows unauthenticated access on a trusted LAN for mobile clients. Do not expose the server to untrusted networks without adding authentication and transport security.
- GraphQL and gRPC are not the current choice. Reconsider them only if future requirements justify their additional complexity or benefits.
