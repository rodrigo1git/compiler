export const messages = {
    1: 'SINGLEF addition overflow (c)',
    2: 'INTEGER multiplication overflow (d)',
    3: 'Loss of fractional information in TOI (g)',
    4: 'TOI value is not representable as INTEGER',
    5: 'Attribute index outside the object layout',
    6: 'Recursive function invocation (i)',
    8: 'Runtime frame memory limit exceeded',
};

export async function execute(bytes, print = console.log) {
    let instance;
    const env = {
        printInteger(value) { print(value); },
        printFloat(value) { print(value); },
        printString(address) {
            const memory = new Uint8Array(instance.exports.memory.buffer);
            let end = address;
            while (end < memory.length && memory[end]) end++;
            print(new TextDecoder().decode(memory.subarray(address, end)));
        },
        runtimeFail(code, line) {
            throw new Error(`Line ${line}: Runtime error: ${messages[code] ?? `Unknown error ${code}`}`);
        },
    };
    ({instance} = await WebAssembly.instantiate(bytes, {env}));
    instance.exports.main();
    return instance;
}
