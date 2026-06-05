# DYA OLED Status Web UI

Web UI for the `dya__oled_status` custom Studio subsystem.

The UI edits OLED status widget visibility, animation, and orientation settings
through ZMK Studio custom RPC.

## Runtime Behavior

- The UI talks to `dya__oled_status`.
- Widget visibility and animation are applied immediately.
- Orientation is saved immediately but applies after reboot.
- RPC operations are serialized and use a bounded timeout, so an unavailable
  subsystem does not leave the UI permanently busy.
- Demo mode never writes to firmware.

## Usage

```sh
npm ci
npm run dev -- --host 127.0.0.1 --port 5173
```

## Build

```sh
npm run build
```

Run `npm run generate` only after changing
`../proto/dya/oled_status/oled_status.proto`.

## Deployment

GitHub Pages deployment is handled by `.github/workflows/deploy-webui.yml`.
The workflow builds `web/` with Node.js 24 and publishes `web/dist`.
