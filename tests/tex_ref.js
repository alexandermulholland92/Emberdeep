/* Reference renderer for the procedural surfaces.
 *
 * Lifts the noise primitives and the surface recipes verbatim out of the
 * browser build (emberdeep.html) and evaluates them with the same
 * height-then-shade order buildSurface() uses, writing raw RGBA. The C
 * port in src/texture.c must reproduce these bytes exactly.
 *
 *   node tests/tex_ref.js <emberdeep.html> <out-dir>
 */
'use strict';
const fs = require('fs');
const path = require('path');

const htmlPath = process.argv[2];
const outDir = process.argv[3];
if (!htmlPath || !outDir) {
  console.error('usage: node tests/tex_ref.js <emberdeep.html> <out-dir>');
  process.exit(2);
}
const html = fs.readFileSync(htmlPath, 'utf8');

/* pull the two contiguous regions we need straight out of the page, so the
   reference is the shipped code rather than a paraphrase of it */
function slice(startMarker, endMarker) {
  const a = html.indexOf(startMarker);
  const b = html.indexOf(endMarker, a);
  if (a < 0 || b < 0) throw new Error('could not locate ' + startMarker);
  return html.slice(a, b);
}
const blockNoise = slice('var TEX_WORLD', 'function finishTexture');
const blockRecipes = slice('function tint(k, r, g, b, out)', 'function surfMat(');

/* buildSurface stands in for the real one: the browser version also derives
   a normal map and a roughness map, and only the colour map is portable to
   the Vita's fixed-function pipeline. The colour path is identical. */
const shim = `
function buildSurface(size, spec) { return { size: size, spec: spec }; }
return buildSurfaces();
`;

const surfaces = new Function(blockNoise + blockRecipes + shim)();

const ORDER = ['floorStone', 'wallMason', 'rock', 'bone', 'cloth',
               'leather', 'metal', 'wood', 'skin', 'hide'];

fs.mkdirSync(outDir, { recursive: true });
for (const name of ORDER) {
  const { size, spec } = surfaces[name];
  const H = new Float64Array(size * size);
  for (let y = 0; y < size; y++)
    for (let x = 0; x < size; x++)
      H[y * size + x] = spec.height(x / size, y / size);

  const cd = new Uint8ClampedArray(size * size * 4);
  const out = [0, 0, 0];
  for (let y = 0; y < size; y++)
    for (let x = 0; x < size; x++) {
      const i = y * size + x;
      spec.shade(H[i], x / size, y / size, out);
      cd[i * 4] = out[0]; cd[i * 4 + 1] = out[1];
      cd[i * 4 + 2] = out[2]; cd[i * 4 + 3] = 255;
    }
  const file = path.join(outDir, name + '.raw');
  fs.writeFileSync(file, Buffer.from(cd.buffer));
  console.log(`${name.padEnd(11)} ${size}x${size}  ${file}`);
}
