declare const __BUILD_ID__: string | undefined;

/** The id of this build (the time it was built); none in the desktop preview, which never reloads itself. */
export const BUILD_ID: string = typeof __BUILD_ID__ === 'undefined' ? '' : __BUILD_ID__;

/** The id of the build now on disk (assets/version.txt, written by the build, see vite.config.ts), or null when it can't be read.
 *  XMLHttpRequest, not fetch: the app is loaded from file://, which fetch refuses. */
export function installedBuild (): Promise<string | null> {
	return new Promise((resolve) => {
		try {
			const xhr = new XMLHttpRequest();
			xhr.open('GET', `assets/version.txt?t=${Date.now()}`);
			xhr.onload = () => resolve(xhr.status === 0 || xhr.status === 200 ? xhr.responseText.trim() || null : null);
			xhr.onerror = () => resolve(null);
			xhr.send();
		} catch { resolve(null); }
	});
}

/** An update was installed while this copy kept running: the TV keeps the launcher (and the Swap's copy of it behind the Home
 *  button) loaded, so without this it would go on showing the old version until the TV restarts. Any other build counts, so a
 *  rebuild of the same version is picked up too. */
export function isOutdated (onDisk: string | null, running: string = BUILD_ID): boolean {
	return !!running && !!onDisk && /^\d+$/.test(onDisk) && onDisk !== running;
}

/** Loads the page again when the files on disk are of another build. */
export function reloadIfUpdated (): void {
	installedBuild().then((v) => { if (isOutdated(v)) window.location.reload(); });
}
