import fs from 'node:fs/promises';
import wabtFactory from 'wabt';

const [input, output = input?.replace(/\.wat$/, '.wasm')] = process.argv.slice(2);
if (!input || !output || input === output) {
    console.error('Usage: node runtime/assemble.mjs input.wat [output.wasm]');
    process.exit(2);
}
let module;
try {
    const wabt = await wabtFactory();
    module = wabt.parseWat(input, await fs.readFile(input, 'utf8'));
    module.resolveNames();
    module.validate();
    const {buffer} = module.toBinary({write_debug_names: true});
    if (!WebAssembly.validate(buffer)) throw new Error('WebAssembly validation failed');
    const temporary = `${output}.tmp.${process.pid}`;
    await fs.writeFile(temporary, buffer);
    await fs.rename(temporary, output);
} catch (error) {
    console.error(error.message);
    process.exitCode = 1;
} finally {
    module?.destroy();
}
