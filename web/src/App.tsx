import { useContext, useMemo, useState } from "react";
import { connect as serialConnect } from "@zmkfirmware/zmk-studio-ts-client/transport/serial";
import { connect as gattConnect } from "@zmkfirmware/zmk-studio-ts-client/transport/gatt";
import {
  ZMKAppContext,
  ZMKConnection,
  ZMKCustomSubsystem,
} from "@cormoran/zmk-studio-react-hook";
import {
  Animation,
  Orientation,
  Request,
  Response,
  type Capabilities,
  type OledStatusConfig,
} from "./proto/dya/oled_status/oled_status";

const SUBSYSTEM_IDENTIFIER = "dya__oled_status";

const DEFAULT_CONFIG: OledStatusConfig = {
  orientation: Orientation.ORIENTATION_LANDSCAPE,
  animation: Animation.ANIMATION_CYBER_FACE,
  showCentralBattery: true,
  showPeripheralBatteries: true,
  showOutput: true,
  showLayer: true,
  showWpm: false,
  showModifiers: true,
  showHidIndicators: true,
};

const DEFAULT_CAPABILITIES: Capabilities = {
  centralBattery: true,
  peripheralBatteries: true,
  output: true,
  layer: true,
  wpm: true,
  modifiers: true,
  hidIndicators: true,
  portrait: true,
  landscape: true,
  animationCyberFace: true,
};

function App() {
  const [demoMode, setDemoMode] = useState(false);

  return (
    <main className="app-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">ZMK custom Studio subsystem</p>
          <h1>OLED RPC</h1>
          <p className="hero-copy">
            Edit OLED status modules, orientation, animation, and visibility at runtime.
          </p>
        </div>
        <button className="secondary" onClick={() => setDemoMode((value) => !value)}>
          {demoMode ? "Disable demo" : "Demo mode"}
        </button>
      </header>

      <ZMKConnection
        renderDisconnected={({ connect, isLoading, error }) => (
          <>
            <section className="panel">
              <h2>Connection</h2>
              {error && <p className="error">{error}</p>}
              <div className="button-row">
                <button disabled={isLoading} onClick={() => connect(gattConnect)}>
                  Connect Bluetooth
                </button>
                <button disabled={isLoading} className="secondary" onClick={() => connect(serialConnect)}>
                  Connect Serial
                </button>
              </div>
            </section>
            {demoMode && <Editor demoMode />}
          </>
        )}
        renderConnected={({ disconnect, deviceName }) => (
          <>
            <section className="panel connection-card">
              <div>
                <h2>Connected</h2>
                <p>{deviceName}</p>
              </div>
              <button className="secondary" onClick={disconnect}>
                Disconnect
              </button>
            </section>
            <Editor demoMode={demoMode} />
          </>
        )}
      />
    </main>
  );
}

function Editor({ demoMode = false }: { demoMode?: boolean }) {
  const zmkApp = useContext(ZMKAppContext);
  const [config, setConfig] = useState<OledStatusConfig>(DEFAULT_CONFIG);
  const [capabilities, setCapabilities] = useState<Capabilities>(DEFAULT_CAPABILITIES);
  const [status, setStatus] = useState(demoMode ? "Demo configuration loaded" : "");
  const [isLoading, setIsLoading] = useState(false);

  const subsystem = useMemo(() => {
    if (demoMode) return null;
    return zmkApp?.findSubsystem(SUBSYSTEM_IDENTIFIER) ?? null;
  }, [demoMode, zmkApp, zmkApp?.state.customSubsystems]);

  const callRpc = async (request: Request): Promise<Response | null> => {
    if (!zmkApp?.state.connection || !subsystem) return null;
    const service = new ZMKCustomSubsystem(zmkApp.state.connection, subsystem.index);
    const payload = Request.encode(Request.create(request)).finish();
    const responsePayload = await service.callRPC(payload);
    if (!responsePayload) return null;
    const response = Response.decode(responsePayload);
    if (response.error) throw new Error(response.error.message || "RPC error");
    return response;
  };

  const load = async () => {
    if (demoMode) {
      setConfig(DEFAULT_CONFIG);
      setCapabilities(DEFAULT_CAPABILITIES);
      setStatus("Demo configuration refreshed");
      return;
    }

    setIsLoading(true);
    try {
      const response = await callRpc({ getConfig: {} });
      if (response?.getConfig?.config) setConfig(response.getConfig.config);
      if (response?.getConfig?.capabilities) setCapabilities(response.getConfig.capabilities);
      setStatus("Loaded OLED configuration");
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "Failed to load configuration");
    } finally {
      setIsLoading(false);
    }
  };

  const save = async (nextConfig = config) => {
    if (demoMode) {
      setConfig(nextConfig);
      setStatus("Demo configuration applied");
      return;
    }

    setIsLoading(true);
    try {
      const response = await callRpc({ setConfig: { config: nextConfig } });
      if (response?.setConfig?.config) setConfig(response.setConfig.config);
      setStatus("Applied to OLED");
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "Failed to apply configuration");
    } finally {
      setIsLoading(false);
    }
  };

  const reset = async () => {
    if (demoMode) {
      setConfig(DEFAULT_CONFIG);
      setStatus("Demo configuration reset");
      return;
    }

    setIsLoading(true);
    try {
      const response = await callRpc({ resetConfig: {} });
      if (response?.resetConfig?.config) setConfig(response.resetConfig.config);
      setStatus("Reset to firmware defaults");
    } catch (error) {
      setStatus(error instanceof Error ? error.message : "Failed to reset configuration");
    } finally {
      setIsLoading(false);
    }
  };

  const update = <K extends keyof OledStatusConfig>(key: K, value: OledStatusConfig[K]) => {
    const nextConfig = { ...config, [key]: value };
    setConfig(nextConfig);
    void save(nextConfig);
  };

  if (!demoMode && !subsystem) {
    return (
      <section className="panel warning">
        <h2>Subsystem not found</h2>
        <p>Firmware must expose custom subsystem `{SUBSYSTEM_IDENTIFIER}`.</p>
      </section>
    );
  }

  return (
    <section className="panel editor-grid">
      <div className="editor-header">
        <div>
          <h2>OLED Modules</h2>
          <p>Select what the OLED status screen should show.</p>
        </div>
        <div className="button-row">
          <button className="secondary" disabled={isLoading} onClick={load}>
            Load
          </button>
          <button className="secondary" disabled={isLoading} onClick={reset}>
            Reset
          </button>
        </div>
      </div>

      <fieldset className="orientation-card">
        <legend>Orientation</legend>
        <label>
          <input
            type="radio"
            checked={config.orientation === Orientation.ORIENTATION_LANDSCAPE}
            disabled={!capabilities.landscape || isLoading}
            onChange={() => update("orientation", Orientation.ORIENTATION_LANDSCAPE)}
          />
          Landscape
        </label>
        <label>
          <input
            type="radio"
            checked={config.orientation === Orientation.ORIENTATION_PORTRAIT}
            disabled={!capabilities.portrait || isLoading}
            onChange={() => update("orientation", Orientation.ORIENTATION_PORTRAIT)}
          />
          Portrait
        </label>
      </fieldset>

      <fieldset className="orientation-card">
        <legend>Animation</legend>
        <label>
          <input
            type="radio"
            checked={config.animation === Animation.ANIMATION_OFF}
            disabled={isLoading}
            onChange={() => update("animation", Animation.ANIMATION_OFF)}
          />
          Off
        </label>
        <label>
          <input
            type="radio"
            checked={config.animation === Animation.ANIMATION_CYBER_FACE}
            disabled={!capabilities.animationCyberFace || isLoading}
            onChange={() => update("animation", Animation.ANIMATION_CYBER_FACE)}
          />
          Cyber face
        </label>
      </fieldset>

      <div className="switch-list">
        <Switch label="Central battery" enabled={capabilities.centralBattery} checked={config.showCentralBattery} onChange={(v) => update("showCentralBattery", v)} />
        <Switch label="Peripheral batteries" enabled={capabilities.peripheralBatteries} checked={config.showPeripheralBatteries} onChange={(v) => update("showPeripheralBatteries", v)} />
        <Switch label="Output" enabled={capabilities.output} checked={config.showOutput} onChange={(v) => update("showOutput", v)} />
        <Switch label="Layer" enabled={capabilities.layer} checked={config.showLayer} onChange={(v) => update("showLayer", v)} />
        <Switch label="WPM" enabled={capabilities.wpm} checked={config.showWpm} onChange={(v) => update("showWpm", v)} />
        <Switch label="Modifiers" enabled={capabilities.modifiers} checked={config.showModifiers} onChange={(v) => update("showModifiers", v)} />
        <Switch label="HID indicators" enabled={capabilities.hidIndicators} checked={config.showHidIndicators} onChange={(v) => update("showHidIndicators", v)} />
      </div>

      {status && <p className="status-line">{status}</p>}
    </section>
  );
}

function Switch({
  label,
  enabled,
  checked,
  onChange,
}: {
  label: string;
  enabled?: boolean;
  checked?: boolean;
  onChange: (value: boolean) => void;
}) {
  return (
    <label className={enabled ? "switch-row" : "switch-row disabled"}>
      <span>{label}</span>
      <input
        type="checkbox"
        checked={Boolean(checked)}
        disabled={!enabled}
        onChange={(event) => onChange(event.target.checked)}
      />
    </label>
  );
}

export default App;
