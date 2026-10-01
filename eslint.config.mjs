// Lint frontendu (TypeScript). Kontrola: make check-lint · oprava, kde jde: npx eslint --fix frontend/src
import js from '@eslint/js';
import tseslint from 'typescript-eslint';

export default tseslint.config(
  { ignores: ['public/dist/**', 'node_modules/**', 'vendor/**', 'venv/**', 'engine/**', 'python/**'] },
  {
    files: ['frontend/src/**/*.ts'],
    extends: [js.configs.recommended, ...tseslint.configs.recommended],
  },
);
