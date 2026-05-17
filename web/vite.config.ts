import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  base: "/zmk-module-oled-rpc/",
  plugins: [react()],
});
