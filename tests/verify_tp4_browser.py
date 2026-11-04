#!/usr/bin/env python3
"""Optional Chromium smoke test exercising the browser's file change handler."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import subprocess
import tempfile
from threading import Thread

ROOT = Path(__file__).resolve().parents[1]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def main():
    chromium = shutil.which('chromium')
    if not chromium:
        raise SystemExit('Chromium is required for this optional browser test')
    with tempfile.TemporaryDirectory(prefix='tp4-browser-', ignore_cleanup_errors=True) as folder:
        directory = Path(folder)
        wat, wasm = directory/'program.wat', directory/'program.wasm'
        result = subprocess.run([str(ROOT/'compiler'),'--wasm','-o',str(wat),
                                 str(ROOT/'tests/tp3_comptime_specialization.txt')],
                                capture_output=True,text=True,check=True)
        assert not result.stderr, result.stderr
        subprocess.run(['node',str(ROOT/'runtime/assemble.mjs'),str(wat),str(wasm)],check=True)
        shutil.copy(ROOT/'runtime/host.mjs',directory/'host.mjs')
        html = (ROOT/'runtime/index.html').read_text() + '''<script type="module">
            const bytes = await (await fetch('/program.wasm')).arrayBuffer();
            const transfer = new DataTransfer();
            transfer.items.add(new File([bytes], 'program.wasm'));
            const input = document.querySelector('#file');
            input.files = transfer.files;
            input.dispatchEvent(new Event('change'));
            </script>'''
        (directory/'index.html').write_text(html)
        server = ThreadingHTTPServer(('127.0.0.1',0),partial(QuietHandler,directory=folder))
        thread = Thread(target=server.serve_forever,daemon=True)
        thread.start()
        try:
            result = subprocess.run([chromium,'--headless','--disable-gpu','--no-sandbox',
                '--disable-dev-shm-usage','--no-first-run','--disable-background-networking',
                '--user-data-dir='+str(directory/'profile'),'--virtual-time-budget=5000',
                '--dump-dom',f'http://127.0.0.1:{server.server_port}/index.html'],
                capture_output=True,text=True,timeout=20)
            assert result.returncode == 0, result.stderr
            assert '<pre id="output" aria-live="polite">3\n7\n3\n</pre>' in result.stdout, result.stdout
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
    print('Browser runner passed: actual file selector handler printed 3, 7, 3.')


if __name__ == '__main__':
    main()
