import http.server
import socketserver
import os

PORT = 8000

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Prevent caching so edits load instantly
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        super().end_headers()

# Explicitly ensure correct MIME types are mapped
MyHTTPRequestHandler.extensions_map.update({
    '.wasm': 'application/wasm',
    '.js': 'application/javascript',
    '.html': 'text/html',
    '.css': 'text/css',
    '.wgsl': 'text/plain',
    '.ttf': 'font/ttf',
})

print(f"Starting server on http://localhost:{PORT}")
os.chdir(os.path.dirname(os.path.abspath(__file__)))
socketserver.ThreadingTCPServer.allow_reuse_address = True
with socketserver.ThreadingTCPServer(("0.0.0.0", PORT), MyHTTPRequestHandler) as httpd:
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.")
