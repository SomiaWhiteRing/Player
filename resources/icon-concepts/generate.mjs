import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = dirname(fileURLToPath(import.meta.url));
const original = readFileSync(resolve(directory, 'easyrpg-original.svg'), 'utf8').trim();
const variants = [
  {
    name: '01-classic', color: '#000000', title: 'Classic calligraphy',
    paths: [
      'M 12,57 C -2,24 42,6 75,23 C 93,33 80,66 68,99 L 43,172 C 38,188 42,199 58,192 C 68,187 73,175 79,173 C 79,192 58,212 35,208 C 9,203 19,180 26,157 L 61,53 C 70,27 41,25 29,40 C 22,49 24,60 31,65 C 23,70 16,66 12,57 Z',
      'M 63,119 C 90,100 112,75 137,44 C 152,25 177,7 193,22 C 210,40 187,62 171,52 C 183,51 186,35 177,34 C 164,32 149,59 125,83 L 94,113 C 121,113 131,135 141,154 C 155,183 172,196 193,182 C 201,176 204,168 207,162 C 218,185 193,210 169,207 C 138,204 124,178 113,153 C 103,132 88,125 63,130 Z',
      'M 1,220 C 54,204 129,214 190,221 C 154,224 65,217 1,226 Z',
    ],
  },
  {
    name: '02-signature', color: '#000000', title: 'Flowing signature',
    paths: [
      'M 8,73 C -7,36 40,12 66,27 C 88,42 70,82 62,108 L 29,205 L 12,211 L 53,81 C 62,54 69,30 48,32 C 29,33 17,52 22,69 Z',
      'M 47,129 C 91,108 109,64 149,29 C 176,5 203,16 193,36 C 188,47 175,49 165,44 C 182,41 184,28 174,28 C 158,26 123,76 101,98 L 74,123 C 103,112 115,123 120,149 C 126,183 138,193 162,187 C 189,180 199,145 198,129 C 220,149 203,185 177,201 C 146,220 116,209 103,180 C 92,155 100,125 67,135 L 47,144 Z',
      'M 0,220 C 60,188 170,241 220,206 C 209,224 183,231 145,227 C 90,220 44,209 0,227 Z',
    ],
  },
  {
    name: '03-gothic', color: '#000000', title: 'Gothic ornamental',
    paths: [
      'M 24,44 L 57,14 L 89,36 L 72,53 L 57,107 L 73,119 L 50,139 L 39,185 L 55,198 L 24,222 L 1,199 L 16,181 L 49,56 L 38,48 L 22,61 Z',
      'M 79,108 L 134,51 L 125,35 L 156,14 L 186,39 L 163,58 L 147,53 L 99,106 L 137,103 L 153,172 L 170,186 L 190,172 L 196,190 L 163,222 L 130,199 L 113,136 L 91,129 L 67,149 L 63,127 Z',
      'M 89,18 C 108,-1 130,6 127,23 C 125,33 113,38 105,34 C 116,32 117,20 110,18 C 103,15 98,20 91,26 Z',
    ],
  },
  {
    name: '04-brush', color: '#000000', title: 'Bold italic brush',
    paths: [
      'M 1,52 C 25,18 59,16 92,27 L 75,56 L 39,188 L 54,194 L 44,210 C 26,218 8,213 1,204 L 45,58 C 27,52 14,57 0,69 Z',
      'M 53,121 C 95,93 122,62 158,24 C 174,15 190,19 202,27 C 178,63 147,93 111,117 C 130,128 134,151 147,169 C 162,190 178,190 201,175 L 208,186 C 193,211 167,220 145,211 C 116,201 105,169 90,148 C 84,140 77,135 65,140 L 48,149 Z',
      'M 48,226 L 190,220 L 169,230 L 43,234 Z',
    ],
  },
];

for (const variant of variants) {
  const shapes = variant.paths.map(d => `<path d="${d}"/>`).join('\n');
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="588" height="588" viewBox="0 0 588 588" role="img" aria-labelledby="title">
<title id="title">EasyRPG K - ${variant.title}</title>
<g transform="translate(8 8) scale(.90)">${original}</g>
<defs><g id="letter">${shapes}</g></defs>
<g transform="translate(342 332)" stroke-linejoin="round" stroke-linecap="round">
<use xlink:href="#letter" fill="#253523" stroke="#253523" stroke-width="18" transform="translate(0 3)"/>
<use xlink:href="#letter" fill="${variant.color}" stroke="#ffffff" stroke-width="12"/>
<use xlink:href="#letter" fill="${variant.color}"/>
</g>
</svg>\n`;
  writeFileSync(resolve(directory, `${variant.name}.svg`), svg);
}
