// WebGPU Infinite Canvas Bridge (No Emscripten)
// Maps C Wasm function calls directly to browser WebGPU APIs

let wgslCode = null;
let mipmapWgslCode = null;

let device = null;
let context = null;
let presentationFormat = null;
let shaderModule = null;
let sampler = null;
let bindGroupLayout = null;
let pipeline = null;
let fontAtlasBitmap = null;

let commandEncoder = null;
let renderPass = null;
let msaaTexture = null;

// Resource tracking
const buffers = [];
const textures = [];
const pipelines = [];

let wasmInstance = null;
let wasmMemory = null;
let currentZoom = 1.0;
let currentPanX = 0.0;
let currentPanY = 0.0;

function numMipLevels(width, height) {
    return 1 + Math.floor(Math.log2(Math.max(width, height)));
}

function updateEditorStyle() {
    const editor = document.getElementById('text-editor');
    if (!editor || editor.style.display !== 'block') return;

    const idxStr = editor.dataset.idx;
    if (idxStr === undefined) return;
    const idx = parseInt(idxStr);

    // Retrieve active properties from WASM
    const x = wasmInstance.exports.get_node_x(idx);
    const y = wasmInstance.exports.get_node_y(idx);
    const w = wasmInstance.exports.get_node_width(idx);
    const h = wasmInstance.exports.get_node_height(idx);
    const wasmFontSize = wasmInstance.exports.get_node_font_size(idx);

    const tr = wasmInstance.exports.get_node_text_r(idx);
    const tg = wasmInstance.exports.get_node_text_g(idx);
    const tb = wasmInstance.exports.get_node_text_b(idx);
    const textColor = rgbToHex(tr, tg, tb);

    // Calculate screen position and dimensions
    const sx = x * currentZoom + currentPanX;
    const sy = y * currentZoom + currentPanY;
    const sWidth = w * currentZoom;
    const sHeight = h * currentZoom;

    // Apply positioning
    editor.style.left = `${sx}px`;
    editor.style.top = `${sy}px`;
    editor.style.width = `${sWidth}px`;
    editor.style.height = `${sHeight}px`;

    // Apply font size and alignment styles (scaled by 1 / 1.26 to match canvas rendering size)
    const fontSize = (wasmFontSize * currentZoom) / 1.26;
    const lineHeight = 1.2 * wasmFontSize * currentZoom; // Exact canvas line height (char_h + line_spacing)
    const lines = editor.value.split('\n');
    const N = lines.length;
    const paddingTop = Math.max(0, (sHeight - N * lineHeight) / 2);

    editor.style.fontSize = `${fontSize}px`;
    editor.style.lineHeight = `${lineHeight}px`;
    editor.style.paddingTop = `${paddingTop}px`;
    editor.style.paddingLeft = `${10 * currentZoom}px`;
    editor.style.paddingRight = `${10 * currentZoom}px`;
    editor.style.paddingBottom = '0px';
    editor.style.margin = '0px';
    editor.style.resize = 'none';
    editor.style.overflow = 'hidden';
    editor.style.color = textColor;
    editor.style.fontFamily = "'Outfit', sans-serif";
    editor.style.fontWeight = '500';
    editor.style.textAlign = 'center';
    editor.style.border = 'none';
    editor.style.background = 'transparent';
    editor.style.boxShadow = 'none';
    editor.style.boxSizing = 'border-box';
}

function getScreenCenterInWorld() {
    const cx = (window.innerWidth / 2.0 - currentPanX) / currentZoom;
    const cy = (window.innerHeight / 2.0 - currentPanY) / currentZoom;
    return { x: cx, y: cy };
}

// Helpers to read/write string to shared Wasm memory
function readString(ptr, len) {
    const view = new Uint8Array(wasmMemory.buffer, ptr, len);
    let actualLen = 0;
    for (let i = 0; i < len; i++) {
        if (view[i] === 0) break;
        actualLen++;
    }
    return new TextDecoder().decode(view.subarray(0, actualLen));
}

function readNullTerminatedString(ptr) {
    const view = new Uint8Array(wasmMemory.buffer, ptr);
    let len = 0;
    while (view[len] !== 0) {
        len++;
    }
    return new TextDecoder().decode(view.subarray(0, len));
}

function writeString(ptr, maxLen, str) {
    const view = new Uint8Array(wasmMemory.buffer, ptr, maxLen);
    const encoded = new TextEncoder().encode(str);
    for (let i = 0; i < maxLen; i++) {
        if (i < encoded.length) {
            view[i] = encoded[i];
        } else {
            view[i] = 0;
        }
    }
}

// Flat API Imports passed to Wasm C Code
const importObject = {
    env: {
        cosf: (x) => Math.cos(x),
        sinf: (x) => Math.sin(x),
        js_console_log: (ptr, len) => console.log(readString(ptr, len)),
        js_console_error: (ptr, len) => console.error(readString(ptr, len)),
        js_random_float: () => Math.random(),
        js_log_click_delta: (delta) => console.log("Click delta time in WASM:", delta),
        js_update_stats: (pan_x, pan_y, zoom, node_count) => {
            currentZoom = zoom;
            currentPanX = pan_x;
            currentPanY = pan_y;
            document.getElementById('stat-pan').innerText = `X: ${Math.round(pan_x)}, Y: ${Math.round(pan_y)}`;
            document.getElementById('stat-zoom').innerText = `${Math.round(zoom * 100)}%`;
            document.getElementById('stat-nodes').innerText = node_count;
        },
        js_set_editing_state: (is_editing, x, y, w, h, current_text_ptr, max_len, idx) => {
            const editor = document.getElementById('text-editor');
            if (is_editing) {
                editor.style.display = 'block';

                // Store parameters on dataset so we can reference them in updateEditorStyle
                editor.dataset.ptr = current_text_ptr;
                editor.dataset.maxLen = max_len;
                editor.dataset.idx = idx;
                editor.dataset.h = h;

                editor.value = readNullTerminatedString(current_text_ptr);
                updateEditorStyle();

                // Delay focus slightly so the browser's default click-focus completes first
                setTimeout(() => {
                    editor.focus();
                }, 50);
            } else {
                editor.style.display = 'none';
            }
        },

        js_init_node_texture: (idx, text_ptr, type, w, h) => {
            // Noop since text is rendered dynamically using the MSDF characters atlas!
        },

        js_wgpu_init: (width, height) => {
            // Already initialized asynchronously in start() before Wasm instantiation
            return 1;
        },
        js_wgpu_create_buffer: (size, usage) => {
            let wgpuUsage = 0;
            if (usage === 1) wgpuUsage = GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST;
            else if (usage === 2) wgpuUsage = GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST;
            else if (usage === 3) wgpuUsage = GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST;

            const buf = device.createBuffer({
                size: size,
                usage: wgpuUsage
            });
            const id = buffers.length;
            buffers.push(buf);
            return id;
        },
        js_wgpu_create_texture: (width, height) => {
            let actualW = width;
            let actualH = height;
            let mips = numMipLevels(actualW, actualH);
            let usage = GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST;
            if (textures.length === 0) {
                actualW = 2048;
                actualH = 2048;
                mips = 1; // Font atlas has only 1 mip level
                usage |= GPUTextureUsage.RENDER_ATTACHMENT; // Required for copyExternalImageToTexture
            }
            const tex = device.createTexture({
                size: [actualW, actualH],
                format: 'rgba8unorm',
                mipLevelCount: mips,
                usage: usage
            });
            const id = textures.length;
            textures.push(tex);
            return id;
        },
        js_wgpu_write_buffer: (buffer_id, offset, data_ptr, size) => {
            const data = new Uint8Array(wasmMemory.buffer, data_ptr, size);
            device.queue.writeBuffer(buffers[buffer_id], offset, data);
        },
        js_wgpu_write_texture: (texture_id, width, height, data_ptr, size) => {
            if (texture_id === 0) {
                // The font atlas is preloaded as font_atlas.png and copied directly using WebGPU
                device.queue.copyExternalImageToTexture(
                    { source: fontAtlasBitmap },
                    { texture: textures[texture_id] },
                    [width, height]
                );
                return;
            }
            const data = new Uint8Array(wasmMemory.buffer, data_ptr, size);
            device.queue.writeTexture(
                { texture: textures[texture_id] },
                data,
                { bytesPerRow: width * 4 },
                [width, height]
            );
        },
        js_wgpu_begin_render_pass: () => {
            commandEncoder = device.createCommandEncoder();
            renderPass = commandEncoder.beginRenderPass({
                colorAttachments: [{
                    view: msaaTexture.createView(),
                    resolveTarget: context.getCurrentTexture().createView(),
                    clearValue: { r: 11 / 255, g: 13 / 255, b: 17 / 255, a: 1.0 }, // Matches --bg-color
                    loadOp: 'clear',
                    storeOp: 'discard'
                }]
            });
        },
        js_wgpu_set_pipeline: (pipeline_id) => {
            renderPass.setPipeline(pipelines[pipeline_id]);
        },
        js_wgpu_set_bind_group: (bind_group_id, uniform_buffer_id, texture_id) => {
            const bg = device.createBindGroup({
                layout: bindGroupLayout,
                entries: [
                    { binding: 0, resource: { buffer: buffers[uniform_buffer_id] } },
                    { binding: 1, resource: textures[texture_id].createView() },
                    { binding: 2, resource: sampler }
                ]
            });
            renderPass.setBindGroup(0, bg);
        },
        js_wgpu_draw_indexed: (index_count, index_start, index_buffer_id, vertex_buffer_id) => {
            renderPass.setVertexBuffer(0, buffers[vertex_buffer_id]);
            renderPass.setIndexBuffer(buffers[index_buffer_id], 'uint32');
            renderPass.drawIndexed(index_count, 1, index_start, 0, 0);
        },
        js_wgpu_end_render_pass: () => {
            renderPass.end();
            device.queue.submit([commandEncoder.finish()]);
        }
    }
};

const editor = document.getElementById('text-editor');

function commitText() {
    console.log('commitText');
    if (editor.style.display === 'block') {
        const ptr = parseInt(editor.dataset.ptr);
        const maxLen = parseInt(editor.dataset.maxLen);
        writeString(ptr, maxLen, editor.value.replace(/\r/g, ''));
        editor.style.display = 'none';
        wasmInstance.exports.on_text_commit();
    }
}

editor.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
        if (e.shiftKey) {
            // Shift+Enter creates a newline naturally
        } else {
            e.preventDefault();
            commitText();
        }
    } else if (e.key === 'Escape') {
        editor.style.display = 'none';
        wasmInstance.exports.on_text_cancel();
    }
});

editor.addEventListener('input', () => {
    updateEditorStyle();
});

editor.addEventListener('blur', () => {
    commitText();
});

let lastSelectedIdx = -1;
let isUpdatingControls = false;

function rgbToHex(r, g, b) {
    const toHex = (c) => {
        const hex = Math.round(c * 255).toString(16);
        return hex.length === 1 ? "0" + hex : hex;
    };
    return "#" + toHex(r) + toHex(g) + toHex(b);
}

function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16) / 255,
        g: parseInt(result[2], 16) / 255,
        b: parseInt(result[3], 16) / 255
    } : null;
}

function updatePropertiesPanel() {
    const selectedIdx = wasmInstance.exports.get_selected_node_idx();
    const panel = document.getElementById('properties-panel');

    if (selectedIdx === -1) {
        panel.style.display = 'none';
        lastSelectedIdx = -1;
        return;
    }

    panel.style.display = 'block';

    // Query parameters from WASM
    const w = wasmInstance.exports.get_node_width(selectedIdx);
    const h = wasmInstance.exports.get_node_height(selectedIdx);
    const type = wasmInstance.exports.get_node_type(selectedIdx);
    const wasmFontSize = wasmInstance.exports.get_node_font_size(selectedIdx);

    const bgR = wasmInstance.exports.get_node_bg_r(selectedIdx);
    const bgG = wasmInstance.exports.get_node_bg_g(selectedIdx);
    const bgB = wasmInstance.exports.get_node_bg_b(selectedIdx);
    const bgA = wasmInstance.exports.get_node_bg_a(selectedIdx);

    const borderR = wasmInstance.exports.get_node_border_r(selectedIdx);
    const borderG = wasmInstance.exports.get_node_border_g(selectedIdx);
    const borderB = wasmInstance.exports.get_node_border_b(selectedIdx);
    const borderA = wasmInstance.exports.get_node_border_a(selectedIdx);

    if (!isUpdatingControls) {
        isUpdatingControls = true;

        const sizeGroup = document.getElementById('size-group');
        const borderGroup = document.getElementById('border-group');
        const bgColorLabel = document.getElementById('bg-color-label');
        const fontGroup = document.getElementById('font-group');

        // Font size control visibility (WIDGET_STICKY, RECT, OVAL, TEXT can have text)
        if (type <= 3) {
            fontGroup.style.display = 'block';
            if (document.activeElement !== document.getElementById('prop-font-size')) {
                document.getElementById('prop-font-size').value = Math.round(wasmFontSize);
            }
        } else {
            fontGroup.style.display = 'none';
        }

        if (type === 5 || type === 6) {
            sizeGroup.style.display = 'none';
            borderGroup.style.display = 'none';
            bgColorLabel.innerText = "Line Color";
        } else {
            sizeGroup.style.display = 'block';
            if (document.activeElement !== document.getElementById('prop-w')) {
                document.getElementById('prop-w').value = Math.round(w);
            }
            if (document.activeElement !== document.getElementById('prop-h')) {
                document.getElementById('prop-h').value = Math.round(h);
            }
            bgColorLabel.innerText = "Background";

            if (type === 3 || type === 4) {
                borderGroup.style.display = 'none';
            } else {
                borderGroup.style.display = 'block';
            }
        }

        const hexBg = rgbToHex(bgR, bgG, bgB);
        document.getElementById('prop-bg-color').value = hexBg;
        document.getElementById('prop-bg-transparent').checked = (bgA <= 0.001);
        document.getElementById('prop-bg-color').disabled = (bgA <= 0.001);

        const hexBorder = rgbToHex(borderR, borderG, borderB);
        document.getElementById('prop-border-color').value = hexBorder;
        document.getElementById('prop-border-transparent').checked = (borderA <= 0.001);
        document.getElementById('prop-border-color').disabled = (borderA <= 0.001);

        // Retrieve and set font color
        const textR = wasmInstance.exports.get_node_text_r(selectedIdx);
        const textG = wasmInstance.exports.get_node_text_g(selectedIdx);
        const textB = wasmInstance.exports.get_node_text_b(selectedIdx);
        document.getElementById('prop-font-color').value = rgbToHex(textR, textG, textB);

        if (type <= 3) {
            document.getElementById('font-color-group').style.display = 'block';
        } else {
            document.getElementById('font-color-group').style.display = 'none';
        }

        isUpdatingControls = false;
    }

    lastSelectedIdx = selectedIdx;
}

function bindPropertiesPanelEvents() {
    const getSelected = () => wasmInstance.exports.get_selected_node_idx();

    const updateSize = () => {
        const idx = getSelected();
        if (idx === -1) return;
        const w = parseFloat(document.getElementById('prop-w').value) || 40;
        const h = parseFloat(document.getElementById('prop-h').value) || 20;
        wasmInstance.exports.set_node_size(idx, w, h);
        updateEditorStyle();
    };

    document.getElementById('prop-w').addEventListener('input', updateSize);
    document.getElementById('prop-h').addEventListener('input', updateSize);

    document.getElementById('prop-font-size').addEventListener('input', () => {
        const idx = getSelected();
        if (idx === -1) return;
        const size = parseFloat(document.getElementById('prop-font-size').value) || 18;
        wasmInstance.exports.set_node_font_size(idx, size);
        updateEditorStyle();
    });

    document.getElementById('prop-font-color').addEventListener('input', () => {
        const idx = getSelected();
        if (idx === -1) return;
        const hex = document.getElementById('prop-font-color').value;
        const rgb = hexToRgb(hex);
        wasmInstance.exports.set_node_text_color(idx, rgb.r, rgb.g, rgb.b);
        updateEditorStyle();
    });

    const updateBg = () => {
        const idx = getSelected();
        if (idx === -1) return;

        const isTransparent = document.getElementById('prop-bg-transparent').checked;
        document.getElementById('prop-bg-color').disabled = isTransparent;

        const hex = document.getElementById('prop-bg-color').value;
        const rgb = hexToRgb(hex);
        const type = wasmInstance.exports.get_node_type(idx);
        const a = isTransparent ? 0.0 : ((type === 5 || type === 6) ? 1.0 : 0.9);

        wasmInstance.exports.set_node_bg_color(idx, rgb.r, rgb.g, rgb.b, a);
        updateEditorStyle();
    };

    document.getElementById('prop-bg-color').addEventListener('input', updateBg);
    document.getElementById('prop-bg-transparent').addEventListener('change', updateBg);

    const updateBorder = () => {
        const idx = getSelected();
        if (idx === -1) return;

        const isNone = document.getElementById('prop-border-transparent').checked;
        document.getElementById('prop-border-color').disabled = isNone;

        const hex = document.getElementById('prop-border-color').value;
        const rgb = hexToRgb(hex);
        const a = isNone ? 0.0 : 1.0;

        wasmInstance.exports.set_node_border_color(idx, rgb.r, rgb.g, rgb.b, a);
    };

    document.getElementById('prop-border-color').addEventListener('input', updateBorder);
    document.getElementById('prop-border-transparent').addEventListener('change', updateBorder);

    document.getElementById('btn-z-front').addEventListener('click', () => {
        const idx = getSelected();
        if (idx !== -1) wasmInstance.exports.bring_to_front_wasm(idx);
    });

    document.getElementById('btn-z-back').addEventListener('click', () => {
        const idx = getSelected();
        if (idx !== -1) wasmInstance.exports.send_to_back_wasm(idx);
    });

    document.getElementById('btn-z-forward').addEventListener('click', () => {
        const idx = getSelected();
        if (idx !== -1) wasmInstance.exports.move_forward_wasm(idx);
    });

    document.getElementById('btn-z-backward').addEventListener('click', () => {
        const idx = getSelected();
        if (idx !== -1) wasmInstance.exports.move_backward_wasm(idx);
    });
}

function updateMsaaTexture() {
    if (!device || !context) return;
    const canvasEl = document.getElementById('canvas');
    if (!canvasEl) return;
    if (msaaTexture) {
        msaaTexture.destroy();
    }
    msaaTexture = device.createTexture({
        size: [canvasEl.width, canvasEl.height],
        sampleCount: 4,
        format: presentationFormat,
        usage: GPUTextureUsage.RENDER_ATTACHMENT
    });
}

// Load the WASM binary and WebGPU shader code
async function start() {
    try {
        const [wasmResponse, wgslResponse, mipmapWgslResponse, fontAtlasImage] = await Promise.all([
            fetch('canvas.wasm'),
            fetch('shader.wgsl'),
            fetch('mipmap.wgsl'),
            new Promise((resolve, reject) => {
                const img = new Image();
                img.onload = () => resolve(img);
                img.onerror = (e) => reject(new Error("Failed to load font_atlas.png"));
                img.src = 'font_atlas.png';
            })
        ]);

        if (!wasmResponse.ok) {
            throw new Error(`Failed to fetch canvas.wasm: ${wasmResponse.statusText}`);
        }
        if (!wgslResponse.ok) {
            throw new Error(`Failed to fetch shader.wgsl: ${wgslResponse.statusText}`);
        }
        if (!mipmapWgslResponse.ok) {
            throw new Error(`Failed to fetch mipmap.wgsl: ${mipmapWgslResponse.statusText}`);
        }

        wgslCode = await wgslResponse.text();
        mipmapWgslCode = await mipmapWgslResponse.text();
        const binary = await wasmResponse.arrayBuffer();
        fontAtlasBitmap = await createImageBitmap(fontAtlasImage, { colorSpaceConversion: 'none' });

        // Initialize WebGPU asynchronously first!
        const canvas = document.getElementById('canvas');
        const width = window.innerWidth;
        const height = window.innerHeight;
        canvas.width = width;
        canvas.height = height;

        if (!navigator.gpu) {
            throw new Error("WebGPU is not supported in this browser. Ensure you are using localhost or HTTPS.");
        }
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
            throw new Error("No WebGPU adapter found.");
        }
        device = await adapter.requestDevice();
        context = canvas.getContext('webgpu');
        presentationFormat = navigator.gpu.getPreferredCanvasFormat();
        context.configure({
            device: device,
            format: presentationFormat,
            alphaMode: 'opaque'
        });

        // Initialize MSAA Texture
        updateMsaaTexture();

        // Compile shader module
        shaderModule = device.createShaderModule({ code: wgslCode });

        // Trilinear sampler for smooth antialiased scaling of text and images at any zoom
        sampler = device.createSampler({
            magFilter: 'linear',
            minFilter: 'linear',
            mipmapFilter: 'linear',
        });

        bindGroupLayout = device.createBindGroupLayout({
            entries: [
                { binding: 0, visibility: GPUShaderStage.VERTEX, buffer: { type: 'uniform' } },
                { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} },
                { binding: 2, visibility: GPUShaderStage.FRAGMENT, sampler: {} }
            ]
        });

        const pipelineLayout = device.createPipelineLayout({
            bindGroupLayouts: [bindGroupLayout]
        });

        pipeline = device.createRenderPipeline({
            layout: pipelineLayout,
            vertex: {
                module: shaderModule,
                entryPoint: 'vs_main',
                buffers: [{
                    arrayStride: 32, // x, y (8) + u, v (8) + r, g, b, a (16)
                    attributes: [
                        { shaderLocation: 0, offset: 0, format: 'float32x2' }, // Position
                        { shaderLocation: 1, offset: 8, format: 'float32x2' }, // UV
                        { shaderLocation: 2, offset: 16, format: 'float32x4' } // Color
                    ]
                }]
            },
            fragment: {
                module: shaderModule,
                entryPoint: 'fs_main',
                targets: [{
                    format: presentationFormat,
                    blend: {
                        color: { srcFactor: 'src-alpha', dstFactor: 'one-minus-src-alpha', operation: 'add' },
                        alpha: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha', operation: 'add' }
                    }
                }]
            },
            primitive: {
                topology: 'triangle-list'
            },
            multisample: {
                count: 4
            }
        });

        pipelines[0] = pipeline;

        // Image pipeline using entry point fs_image
        const pipelineImage = device.createRenderPipeline({
            layout: pipelineLayout,
            vertex: {
                module: shaderModule,
                entryPoint: 'vs_main',
                buffers: [{
                    arrayStride: 32,
                    attributes: [
                        { shaderLocation: 0, offset: 0, format: 'float32x2' },
                        { shaderLocation: 1, offset: 8, format: 'float32x2' },
                        { shaderLocation: 2, offset: 16, format: 'float32x4' }
                    ]
                }]
            },
            fragment: {
                module: shaderModule,
                entryPoint: 'fs_image',
                targets: [{
                    format: presentationFormat,
                    blend: {
                        color: { srcFactor: 'src-alpha', dstFactor: 'one-minus-src-alpha', operation: 'add' },
                        alpha: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha', operation: 'add' }
                    }
                }]
            },
            primitive: {
                topology: 'triangle-list'
            },
            multisample: {
                count: 4
            }
        });
        pipelines[1] = pipelineImage;

        // Define an initial memory allocation (64KB pages)
        // 256 pages = 16MB of shared heap memory
        const memory = new WebAssembly.Memory({ initial: 256 });
        wasmMemory = memory;
        importObject.js_wgpu_memory = memory; // Pass reference or make it available

        // Inject shared memory into import environment
        importObject.env.memory = memory;

        // Ensure the browser has fully downloaded/loaded the web font Outfit.
        // We trigger download in the background and use Promise.race with a fast 200ms timeout
        // to avoid locking/freezing the initialization block.
        try {
            document.fonts.load('500 12px Outfit').catch(e => console.error("Error background loading Outfit:", e));
            await Promise.race([
                document.fonts.ready,
                new Promise(resolve => setTimeout(resolve, 200))
            ]);
        } catch (e) {
            console.error("Error loading Outfit font:", e);
        }

        const { instance } = await WebAssembly.instantiate(binary, importObject);
        wasmInstance = instance;
        wasmMemory = instance.exports.memory;

        // Initialize application layout and inputs
        const success = instance.exports.init_app(width, height);
        if (success === 0) {
            throw new Error("C engine initialization failed.");
        }

        setupInputHandlers();
        bindPropertiesPanelEvents();

        // Hide loader panel
        document.getElementById('loader').style.opacity = '0';
        setTimeout(() => { document.getElementById('loader').style.display = 'none'; }, 500);

        // Start Render Loop
        function step(timestamp) {
            instance.exports.tick_app(timestamp);
            updatePropertiesPanel();
            updateEditorStyle();
            requestAnimationFrame(step);
        }
        requestAnimationFrame(step);

    } catch (err) {
        console.error(err);
        document.getElementById('loader').style.display = 'block';
        document.getElementById('error-msg').innerText = err.message;
    }
}

function setupInputHandlers() {
    const canvas = document.getElementById('canvas');

    let isDrawMode = false;
    let isDrawingStroke = false;
    let isArrowMode = false;

    function toggleDrawMode(active) {
        isDrawMode = active;
        const btn = document.getElementById('btn-toggle-draw');
        const txt = document.getElementById('draw-btn-text');
        if (isDrawMode) {
            if (isArrowMode) toggleArrowMode(false);
            if (txt) txt.innerText = "Draw Tool: ON";
            btn.title = "Draw Tool: ON (Click to turn OFF)";
            btn.style.background = 'var(--accent)';
            btn.style.color = '#fff';
            btn.style.borderColor = 'transparent';
            btn.style.boxShadow = '0 0 15px var(--accent-glow)';
        } else {
            if (txt) txt.innerText = "Draw Tool: OFF";
            btn.title = "Toggle Draw Tool (Freehand drawing)";
            btn.style.background = '';
            btn.style.color = '';
            btn.style.borderColor = '';
            btn.style.boxShadow = '';
        }
        isDrawingStroke = false;
    }

    function toggleArrowMode(active) {
        isArrowMode = active;
        const btn = document.getElementById('btn-toggle-connection');
        const txt = document.getElementById('connection-btn-text');
        if (isArrowMode) {
            if (isDrawMode) toggleDrawMode(false);
            if (txt) txt.innerText = "Connection Tool: ON";
            btn.title = "Connection Tool: ON (Click to turn OFF)";
            btn.style.background = 'var(--accent)';
            btn.style.color = '#fff';
            btn.style.borderColor = 'transparent';
            btn.style.boxShadow = '0 0 15px var(--accent-glow)';
            canvas.style.cursor = 'crosshair';
            wasmInstance.exports.set_arrow_tool(1);
        } else {
            if (txt) txt.innerText = "Connection Tool: OFF";
            btn.title = "Toggle Connection Tool (Shift + click shape to connect)";
            btn.style.background = '';
            btn.style.color = '';
            btn.style.borderColor = '';
            btn.style.boxShadow = '';
            canvas.style.cursor = '';
            wasmInstance.exports.set_arrow_tool(0);
        }
    }

    function getCanvasCoords(e) {
        const rect = canvas.getBoundingClientRect();
        return {
            x: e.clientX - rect.left,
            y: e.clientY - rect.top
        };
    }

    function getWorldCoords(clientX, clientY) {
        const rect = canvas.getBoundingClientRect();
        const x = clientX - rect.left;
        const y = clientY - rect.top;
        const wx = (x - currentPanX) / currentZoom;
        const wy = (y - currentPanY) / currentZoom;
        return { x: wx, y: wy };
    }

    canvas.addEventListener('mousedown', (e) => {
        if (e.button === 2) {
            e.preventDefault();
        }
        commitText();

        if (isDrawMode && e.button === 0) {
            isDrawingStroke = true;
            const wCoords = getWorldCoords(e.clientX, e.clientY);
            const hex = document.getElementById('draw-color-picker').value || '#1accde';
            const rgb = hexToRgb(hex);
            wasmInstance.exports.start_stroke(wCoords.x, wCoords.y, rgb.r, rgb.g, rgb.b);
            e.preventDefault();
            return;
        }

        const coords = getCanvasCoords(e);
        const forceShift = isArrowMode ? 1 : (e.shiftKey ? 1 : 0);
        const beforeCount = wasmInstance.exports.get_node_count();

        wasmInstance.exports.on_mouse_down(e.button, coords.x, coords.y, forceShift, (e.ctrlKey || e.metaKey) ? 1 : 0);

        const afterCount = wasmInstance.exports.get_node_count();
        if (isArrowMode && afterCount > beforeCount) {
            toggleArrowMode(false);
        }
    });

    window.addEventListener('mousemove', (e) => {
        if (isDrawMode && isDrawingStroke) {
            const wCoords = getWorldCoords(e.clientX, e.clientY);
            wasmInstance.exports.add_stroke_point(wCoords.x, wCoords.y);
            return;
        }
        const coords = getCanvasCoords(e);
        wasmInstance.exports.on_mouse_move(coords.x, coords.y);
    });

    window.addEventListener('mouseup', (e) => {
        if (isDrawMode && isDrawingStroke) {
            wasmInstance.exports.end_stroke();
            isDrawingStroke = false;
            return;
        }
        const coords = getCanvasCoords(e);
        wasmInstance.exports.on_mouse_up(e.button, coords.x, coords.y);
    });

    canvas.addEventListener('wheel', (e) => {
        e.preventDefault();
        const coords = getCanvasCoords(e);
        wasmInstance.exports.on_mouse_wheel(e.deltaY, coords.x, coords.y);
    }, { passive: false });

    canvas.addEventListener('contextmenu', (e) => {
        e.preventDefault();
    });

    window.addEventListener('resize', () => {
        const w = window.innerWidth;
        const h = window.innerHeight;
        canvas.width = w;
        canvas.height = h;
        updateMsaaTexture();
        wasmInstance.exports.on_resize(w, h);
    });

    window.addEventListener('keydown', (e) => {
        if (document.activeElement === editor) {
            return;
        }

        let keyCode = 0;
        if (e.key === 'Backspace') keyCode = 8;
        else if (e.key === 'Delete') keyCode = 46;
        else if (e.key === ' ') keyCode = 32;

        if (keyCode !== 0) {
            wasmInstance.exports.on_key_down(keyCode);
        }
    });

    window.addEventListener('keyup', (e) => {
        if (document.activeElement === editor) {
            return;
        }
        if (e.key === ' ') {
            wasmInstance.exports.on_key_up(32);
        }
    });

    document.getElementById('btn-bulk-create').addEventListener('click', () => {
        commitText();
        if (confirm("Create 100,000 infographics? This will reset the board.")) {
            wasmInstance.exports.create_100k_infographics();
        }
    });

    document.getElementById('btn-add-sticky').addEventListener('click', () => {
        commitText();
        const center = getScreenCenterInWorld();
        wasmInstance.exports.add_widget_wasm(0, center.x, center.y, -1, 0, 0);
    });

    document.getElementById('btn-add-rect').addEventListener('click', () => {
        commitText();
        const center = getScreenCenterInWorld();
        wasmInstance.exports.add_widget_wasm(1, center.x, center.y, -1, 0, 0);
    });

    document.getElementById('btn-add-oval').addEventListener('click', () => {
        commitText();
        const center = getScreenCenterInWorld();
        wasmInstance.exports.add_widget_wasm(2, center.x, center.y, -1, 0, 0);
    });

    document.getElementById('btn-add-text').addEventListener('click', () => {
        commitText();
        const center = getScreenCenterInWorld();
        wasmInstance.exports.add_widget_wasm(3, center.x, center.y, -1, 0, 0);
    });

    const imageLoader = document.getElementById('image-loader');
    document.getElementById('btn-add-image').addEventListener('click', () => {
        commitText();
        imageLoader.click();
    });

    imageLoader.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;

        const reader = new FileReader();
        reader.onload = (event) => {
            const img = new Image();
            img.onload = () => {
                const tempCanvas = document.createElement('canvas');
                tempCanvas.width = img.width;
                tempCanvas.height = img.height;
                const tempCtx = tempCanvas.getContext('2d');
                tempCtx.drawImage(img, 0, 0);
                const imgData = tempCtx.getImageData(0, 0, img.width, img.height);

                const imgMips = numMipLevels(img.width, img.height);
                const texture = device.createTexture({
                    size: [img.width, img.height],
                    format: 'rgba8unorm',
                    mipLevelCount: imgMips,
                    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT
                });

                device.queue.writeTexture(
                    { texture: texture },
                    imgData.data,
                    { bytesPerRow: img.width * 4 },
                    [img.width, img.height]
                );
                generateMipmaps(texture, img.width, img.height);

                const textureId = textures.length;
                textures.push(texture);

                const center = getScreenCenterInWorld();
                wasmInstance.exports.add_widget_wasm(4, center.x, center.y, textureId, img.width, img.height);
                imageLoader.value = '';
            };
            img.src = event.target.result;
        };
        reader.readAsDataURL(file);
    });

    const toggleDrawBtn = document.getElementById('btn-toggle-draw');
    toggleDrawBtn.addEventListener('click', () => {
        commitText();
        toggleDrawMode(!isDrawMode);
    });

    const toggleConnectionBtn = document.getElementById('btn-toggle-connection');
    toggleConnectionBtn.addEventListener('click', () => {
        commitText();
        toggleArrowMode(!isArrowMode);
    });

    document.getElementById('btn-clear').addEventListener('click', () => {
        console.log("Clear Board button clicked in JS!");
        commitText();
        if (confirm("Clear all widgets and connections?")) {
            try {
                wasmInstance.exports.on_btn_clear_click();
                console.log("on_btn_clear_click finished calling WASM");
            } catch (e) {
                console.error("WASM on_btn_clear_click crashed:", e);
            }
        }
    });
}

// Start execution
start();
