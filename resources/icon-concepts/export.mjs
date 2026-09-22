import { copyFileSync, readFileSync, writeFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = dirname(fileURLToPath(import.meta.url));
const root = resolve(directory, '../..');
const source = resolve(directory, '03-gothic.svg');
const raster = ['-background', 'none', '-density', '288', source];
const targets = [
  ['resources/unix/icon-48.png', 48],
  ['builds/android/metadata/en-US/images/icon.png', 512],
  ...Object.entries({ mdpi: 48, hdpi: 72, xhdpi: 96, xxhdpi: 144, xxxhdpi: 192 })
    .map(([density, size]) => [`builds/android/app/src/main/res/drawable-${density}/ic_launcher.png`, size]),
];

copyFileSync(source, resolve(root, 'resources/unix/easyrpg-player.svg'));
for (const [file, size] of targets) {
  execFileSync('magick', [...raster, '-resize', `${size}x${size}`, '-depth', '8', `PNG32:${resolve(root, file)}`]);
}
execFileSync('magick', [...raster, '-resize', '256x256', '-depth', '8', '-define', 'icon:auto-resize=256,128,64,48,32,16', resolve(root, 'resources/windows/player.ico')]);

const pixels = execFileSync('magick', [...raster, '-resize', '32x32', '-depth', '8', 'RGBA:-']);
if (pixels.length !== 32 * 32 * 4) throw new Error('Unexpected RGBA icon size');
const rows = [];
for (let i = 0; i < pixels.length; i += 12) {
  rows.push('  ' + Array.from(pixels.subarray(i, i + 12), value => `0x${value.toString(16).padStart(2, '0')}`).join(', ') + ',');
}
const headerPath = resolve(root, 'src/icon.h');
const header = readFileSync(headerPath, 'utf8');
if (!/uint8_t icon32\[\] = \{[\s\S]*?\};/.test(header)) throw new Error('Icon array not found');
writeFileSync(headerPath, header.replace(/uint8_t icon32\[\] = \{[\s\S]*?\};/, `uint8_t icon32[] = {\n${rows.join('\n')}\n};`));
