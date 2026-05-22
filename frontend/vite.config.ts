import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

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
      "/api": "http://127.0.0.1:8000",
      "/assets": "http://127.0.0.1:8000"
    }
  }
});
