import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// Determine API target based on environment
// In Docker: use backend service name
// Local: use localhost
const apiTarget = (() => {
  if (process.env.VITE_API_TARGET) {
    return process.env.VITE_API_TARGET;
  }
  // Default to backend service in Docker Compose network
  return "http://backend:8000";
})();

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
