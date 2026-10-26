// Writes repo/apps.json: the tiny "repository" file that the webOS Homebrew Channel can read
// (Settings -> Add repository). It lists this one app, with the exact version, download address and
// SHA-256 of the .ipk attached to the GitHub release, so the Homebrew Channel can show and install updates.
//
//   node scripts/gen-repo.mjs --repo <owner>/<name> --ipk <path to the .ipk>
//
// Run it again for every new release (the hash must match the exact .ipk you attach to the release).
import {createHash} from 'node:crypto';
import {mkdirSync, readFileSync, statSync, writeFileSync} from 'node:fs';
import {basename} from 'node:path';

const arg = (name) => {
	const i = process.argv.indexOf(`--${name}`);
	return i === -1 ? undefined : process.argv[i + 1];
};

const repo = arg('repo');
const ipk = arg('ipk');
if (!repo || !ipk) {
	console.error('Usage: node scripts/gen-repo.mjs --repo <owner>/<name> --ipk <file.ipk>');
	process.exit(1);
}

const appinfo = JSON.parse(readFileSync('public/appinfo.json', 'utf8'));
const expected = `${appinfo.id}_${appinfo.version}_all.ipk`;
if (basename(ipk) !== expected) {
	console.error(`The file should be called ${expected}`);
	process.exit(1);
}
const data = readFileSync(ipk);
const raw = `https://raw.githubusercontent.com/${repo}/main`;

const manifest = {
	id: appinfo.id,
	version: appinfo.version,
	type: appinfo.type,
	title: appinfo.title,
	appDescription: 'A quieter home screen for LG TVs: clock, weather, your calendar and a row of apps over your wallpaper.',
	iconUri: `${raw}/public/largeIcon.png`,
	sourceUrl: `https://github.com/${repo}`,
	rootRequired: 'optional',
	ipkUrl: `https://github.com/${repo}/releases/download/v${appinfo.version}/${expected}`,
	ipkHash: {sha256: createHash('sha256').update(data).digest('hex')},
	ipkSize: statSync(ipk).size
};

const file = {
	paging: {page: 1, count: 1, maxPage: 1, itemsTotal: 1},
	packages: [{
		id: appinfo.id,
		title: appinfo.title,
		iconUri: manifest.iconUri,
		shortDescription: 'Clock, weather, calendar and your apps over your wallpaper. A fork of Hearth.',
		// the longer text shown on the app's page in the Homebrew Channel (repo/description.html, simple HTML)
		fullDescriptionUrl: `${raw}/repo/description.html`,
		manifest
	}]
};

mkdirSync('repo', {recursive: true});
writeFileSync('repo/apps.json', JSON.stringify(file, null, 2) + '\n');
console.log(`Wrote repo/apps.json for version ${appinfo.version}, sha256 ${manifest.ipkHash.sha256}`);
