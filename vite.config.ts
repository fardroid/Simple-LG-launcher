import {defineConfig, type Plugin} from 'vite';
import preact from '@preact/preset-vite';

// This fork targets Chrome 53 (webOS 4.x TVs; webOS 6 runs Chrome 79 and newer, which runs it too).
// Two things matter for these engines:
//  1. Module scripts (`<script type="module">`) are refused when loaded from file://
//     because the sandbox serves them with no MIME type, so the bundle must be a
//     classic script.
//  2. Newer syntax (`?.`, `??`, object spread, optional `catch`) is a SyntaxError, so
//     everything (dependencies included) is compiled down to the chrome53 target.
const BROWSER_TARGET = 'chrome53';

/** Rewrites Vite's module entry tag into a classic deferred script tag. */
function classicScriptTag (): Plugin {
	return {
		name: 'classic-script-tag',
		enforce: 'post',
		transformIndexHtml (html) {
			return html
				.replace(/<script type="module" crossorigin src="([^"]+)"><\/script>/g, '<script defer src="$1"></script>')
				.replace(/<link rel="stylesheet" crossorigin href="/g, '<link rel="stylesheet" href="');
		}
	};
}

// HEARTH_WEATHER=0 builds without the weather widget (the code is removed from the bundle).
const env = (globalThis as unknown as {process?: {env: Record<string, string | undefined>}}).process;
const WEATHER = !(env && env.env.HEARTH_WEATHER === '0');

export default defineConfig({
	define: {__WEATHER__: JSON.stringify(WEATHER)},
	base: './',            // the app is loaded from a local file path on the TV
	plugins: [preact(), classicScriptTag()],
	build: {
		target: BROWSER_TARGET,
		outDir: 'dist',
		modulePreload: false,
		cssTarget: BROWSER_TARGET,
		rollupOptions: {
			output: {
				format: 'iife',
				inlineDynamicImports: true,
				entryFileNames: 'assets/app.js',
				assetFileNames: 'assets/[name][extname]'
			}
		}
	}
});
