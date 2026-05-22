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
                    radix: [
                        "@radix-ui/react-scroll-area", 
                        "@radix-ui/react-select", 
                        "@radix-ui/react-slot", 
                        "@radix-ui/react-switch"
                    ],
                    icons: ["lucide-react"]
                }
            }
        }
    },
    server: {
        port: 5173,
        proxy: {
            // Меняем 127.0.0.1 на имя сервиса бэкенда в Docker Compose
            "/api": {
                target: "http://backend:8000",
                changeOrigin: true
            },
            "/assets": {
                target: "http://backend:8000",
                changeOrigin: true
            }
        }
    }
});