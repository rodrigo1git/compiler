import fs from 'node:fs/promises';
import {execute} from './host.mjs';
const args = process.argv.slice(2);
const json = args[0] === '--json';
const input = args[json ? 1 : 0];
if (!input) { console.error('Usage: node runtime/run.mjs [--json] program.wasm'); process.exit(2); }
const output = [];
try {
    await execute(await fs.readFile(input), value => {
        if (json) output.push(value); else console.log(value);
    });
} catch (error) {
    console.error(error.message);
    process.exitCode = 1;
} finally {
    if (json) console.log(JSON.stringify(output));
}
