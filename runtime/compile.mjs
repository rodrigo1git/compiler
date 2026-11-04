import path from 'node:path';
import {statSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const args = process.argv.slice(2);
let input, output, execute = false;
for (let index = 0; index < args.length; index++) {
    if (args[index] === '--run') execute = true;
    else if (args[index] === '-o' && index + 1 < args.length) output = path.resolve(args[++index]);
    else if (!input && !args[index].startsWith('-')) input = path.resolve(args[index]);
    else { console.error('Usage: ./compile_wasm.sh source [-o output.wat] [--run]'); process.exit(2); }
}
if (!input) { console.error('Missing source file'); process.exit(2); }
output ??= `${input}.wat`;
if (!output.endsWith('.wat')) { console.error('Output path must end in .wat'); process.exit(2); }
const binary = output.slice(0,-4) + '.wasm';
for (const target of [output, binary]) {
    let identical = target === input;
    try {
        const sourceStat = statSync(input), targetStat = statSync(target);
        identical ||= sourceStat.dev === targetStat.dev && sourceStat.ino === targetStat.ino;
    } catch { /* Nonexistent outputs are created after successful compilation. */ }
    if (identical) { console.error('Output path must differ from the source file'); process.exit(1); }
}
function run(command, args) {
    const result = spawnSync(command, args, {stdio: 'inherit'});
    if (result.error) { console.error(result.error.message); process.exit(1); }
    if (result.status !== 0) process.exit(result.status ?? 1);
}
run(path.join(root, 'compiler'), ['--wasm', '-o', output, input]);
run(process.execPath, [path.join(root,'runtime/assemble.mjs'), output, binary]);
console.log(`WebAssembly: ${binary}`);
if (execute) run(process.execPath, [path.join(root,'runtime/run.mjs'), binary]);
