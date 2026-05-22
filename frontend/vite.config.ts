import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// Use backend service name in Docker, localhost for local dev
const apiTarget = process.env.VITE_API_TARGET || "http://127.0.0.1:8000";

export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          board: ["react-chessboard", "chess.js"],
          motion: ["framer-motion"],
          radix: ["@radix-ui/react-scroll-area", "@radix-ui/react-select", "@radix-ui/react-slot", "@radix-ui/react-switch"],
          icons: ["lucide-react"]
        }
      }
    }
  },
  server: {
    port: 5173,
    proxy: {
      "/api": apiTarget,
      "/assets": apiTarget
    }
  }
});
