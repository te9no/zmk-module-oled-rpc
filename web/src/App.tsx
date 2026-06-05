import { useContext, useMemo, useRef, useState } from "react";
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
const RPC_TIMEOUT_MS = 7000;

type StatusKind = "info" | "success" | "error";

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
            Edit OLED status modules, animation, and visibility at runtime.
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
  const [statusKind, setStatusKind] = useState<StatusKind>(demoMode ? "success" : "info");
  const [isLoading, setIsLoading] = useState(false);
  const operationRef = useRef<string | null>(null);

  const subsystem = useMemo(() => {
    if (demoMode) return null;
    return zmkApp?.findSubsystem(SUBSYSTEM_IDENTIFIER) ?? null;
  }, [demoMode, zmkApp, zmkApp?.state.customSubsystems]);

  const setStatusMessage = (message: string, kind: StatusKind = "info") => {
    setStatus(message);
    setStatusKind(kind);
  };

  const startOperation = (label: string) => {
    if (operationRef.current) {
      setStatusMessage(`${operationRef.current} is still running. Please wait a moment.`);
      return false;
    }

    operationRef.current = label;
    setIsLoading(true);
    setStatusMessage(label);
    return true;
  };

  const finishOperation = () => {
    operationRef.current = null;
    setIsLoading(false);
  };

  const callRpc = async (request: Request): Promise<Response> => {
    if (!zmkApp?.state.connection || !subsystem) {
      throw new Error("OLED status subsystem is not available");
    }
    const service = new ZMKCustomSubsystem(zmkApp.state.connection, subsystem.index);
    const payload = Request.encode(Request.create(request)).finish();
    const responsePayload = await service.callRPC(payload, { timeout: RPC_TIMEOUT_MS });
    if (!responsePayload) throw new Error("Empty RPC response");
    const response = Response.decode(responsePayload);
    if (response.error) throw new Error(response.error.message || "RPC error");
    return response;
  };

  const load = async () => {
    if (demoMode) {
      setConfig(DEFAULT_CONFIG);
      setCapabilities(DEFAULT_CAPABILITIES);
      setStatusMessage("Demo configuration refreshed", "success");
      return;
    }

    if (!startOperation("Loading OLED configuration...")) return;
    try {
      const response = await callRpc({ getConfig: {} });
      if (response.getConfig?.config) setConfig(response.getConfig.config);
      if (response.getConfig?.capabilities) setCapabilities(response.getConfig.capabilities);
      setStatusMessage("Loaded OLED configuration", "success");
    } catch (error) {
      setStatusMessage(error instanceof Error ? error.message : "Failed to load configuration", "error");
    } finally {
      finishOperation();
    }
  };

  const save = async (nextConfig = config, requiresReboot = false) => {
    if (demoMode) {
      setConfig(nextConfig);
      setStatusMessage(
        requiresReboot ? "Demo orientation saved; reboot would apply it" : "Demo configuration applied",
        "success",
      );
      return;
    }

    if (!startOperation("Saving OLED configuration...")) return;
    try {
      const response = await callRpc({ setConfig: { config: nextConfig } });
      if (response.setConfig?.config) setConfig(response.setConfig.config);
      setStatusMessage(
        requiresReboot ? "Orientation saved. Reboot the keyboard to apply it." : "Applied to OLED",
        requiresReboot ? "info" : "success",
      );
    } catch (error) {
      setStatusMessage(error instanceof Error ? error.message : "Failed to apply configuration", "error");
    } finally {
      finishOperation();
    }
  };

  const reset = async () => {
    if (demoMode) {
      setConfig(DEFAULT_CONFIG);
      setStatusMessage("Demo configuration reset", "success");
      return;
    }

    if (!startOperation("Resetting OLED configuration...")) return;
    try {
      const response = await callRpc({ resetConfig: {} });
      const resetConfig = response.resetConfig?.config;
      if (resetConfig) {
        setConfig(resetConfig);
        setStatusMessage(
          resetConfig.orientation !== config.orientation
            ? "Reset saved. Reboot the keyboard to apply orientation."
            : "Reset to firmware defaults",
          resetConfig.orientation !== config.orientation ? "info" : "success",
        );
      } else {
        setStatusMessage("Reset to firmware defaults", "success");
      }
    } catch (error) {
      setStatusMessage(error instanceof Error ? error.message : "Failed to reset configuration", "error");
    } finally {
      finishOperation();
    }
  };

  const update = <K extends keyof OledStatusConfig>(key: K, value: OledStatusConfig[K]) => {
    const nextConfig = { ...config, [key]: value };
    setConfig(nextConfig);
    void save(nextConfig, key === "orientation");
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
        <p className="hint">Orientation is saved immediately and applied after reboot.</p>
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
        <Switch label="Central battery" enabled={capabilities.centralBattery && !isLoading} checked={config.showCentralBattery} onChange={(v) => update("showCentralBattery", v)} />
        <Switch label="Peripheral batteries" enabled={capabilities.peripheralBatteries && !isLoading} checked={config.showPeripheralBatteries} onChange={(v) => update("showPeripheralBatteries", v)} />
        <Switch label="Output" enabled={capabilities.output && !isLoading} checked={config.showOutput} onChange={(v) => update("showOutput", v)} />
        <Switch label="Layer" enabled={capabilities.layer && !isLoading} checked={config.showLayer} onChange={(v) => update("showLayer", v)} />
        <Switch label="WPM" enabled={capabilities.wpm && !isLoading} checked={config.showWpm} onChange={(v) => update("showWpm", v)} />
        <Switch label="Modifiers" enabled={capabilities.modifiers && !isLoading} checked={config.showModifiers} onChange={(v) => update("showModifiers", v)} />
        <Switch label="HID indicators" enabled={capabilities.hidIndicators && !isLoading} checked={config.showHidIndicators} onChange={(v) => update("showHidIndicators", v)} />
      </div>

      {status && <p className={`status-line ${statusKind}`}>{status}</p>}
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
