import { defineConfig } from 'vite';
import { resolve } from 'path';

export default defineConfig({
    root: 'frontend',
    build: {
        outDir: '../public/dist',
        emptyOutDir: true,
        rollupOptions: {
            input: {
                main: resolve(__dirname, 'frontend/src/main.ts'),
                match: resolve(__dirname, 'frontend/src/match.ts'),
            },
            output: {
                entryFileNames: '[name].js',
                chunkFileNames: '[name].js',
                assetFileNames: '[name].[ext]',
            },
        },
    },
    server: {
        proxy: {
            '/api': 'http://localhost:8080',
        },
    },
    resolve: {
        alias: {
            '@': resolve(__dirname, 'frontend/src'),
        },
    },
});
