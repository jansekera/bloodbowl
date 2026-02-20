import { defineConfig } from 'vitest/config';
import { resolve } from 'path';

export default defineConfig({
    test: {
        include: ['frontend/src/**/*.test.ts'],
        environment: 'node',
    },
    resolve: {
        alias: {
            '@': resolve(__dirname, 'frontend/src'),
        },
    },
});
