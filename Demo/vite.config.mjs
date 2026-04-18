import { defineConfig } from "vite";

export default defineConfig(() => {
  return {
    base: "./",
    build: {
      sourcemap: true,
      outDir: "./dist",
      emptyOutDir: true,
      chunkSizeWarningLimit: 1000,
    },
  };
});
