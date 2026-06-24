# Infinity Canvas

[![Deploy to GitHub Pages](https://github.com/vteremasov/dust/actions/workflows/deploy.yml/badge.svg)](https://github.com/vteremasov/dust/actions/workflows/deploy.yml)

An interactive, high-performance infinite canvas built with WebGPU and pure C, compiled to WebAssembly. 

## Features
- **Shapes & Sticky Notes**: Place and edit sticky notes, rectangles, ovals, and text labels.
- **Connections**: Connect nodes together dynamically.
- **Freehand Drawing**: Toggle drawing tool to sketch directly on the canvas.
- **Image Uploads**: Place and render images onto the canvas.
- **Performance**: High-fidelity WebGPU rendering engine written in pure C.

## Local Setup

### Prerequisites
To build the project locally, you need a compiler that can target WebAssembly (e.g. `clang` with WASM target support).

### Building
Compile the WebAssembly binary:
```bash
./build.sh
```

### Running Locally
Run the Python development server:
```bash
python3 server.py
```
Then navigate to `http://localhost:8000`.

## Deployment
Every push to the `main` branch automatically compiles the WebAssembly binary and deploys the static files to GitHub Pages via GitHub Actions.
