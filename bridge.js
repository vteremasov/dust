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
let lastPanText = "";
let lastZoomText = "";
let lastNodeCount = -1;
let lastProps = {
    selectedIdx: -1,
    w: -1,
    h: -1,
    type: -1,
    fontSize: -1,
    bgR: -1, bgG: -1, bgB: -1, bgA: -1,
    borderR: -1, borderG: -1, borderB: -1, borderA: -1,
    textR: -1, textG: -1, textB: -1
};



let isDebugExpanded = false;
let frameCountForFps = 0;
let currentFps = 60;
let lastFpsUpdateTime = performance.now();
let lastDebugPanelUpdateTime = 0;

function numMipLevels(width, height) {
    return 1 + Math.floor(Math.log2(Math.max(width, height)));
}

let mipmapPipeline = null;
let mipmapSampler = null;

function generateMipmaps(texture, width, height) {
    const mipCount = numMipLevels(width, height);
    if (mipCount <= 1) return;

    if (!mipmapPipeline) {
        const mipmapShaderModule = device.createShaderModule({
            code: mipmapWgslCode
        });

        mipmapSampler = device.createSampler({
            minFilter: 'linear',
            magFilter: 'linear',
        });

        mipmapPipeline = device.createRenderPipeline({
            layout: 'auto',
            vertex: {
                module: mipmapShaderModule,
                entryPoint: 'vs',
            },
            fragment: {
                module: mipmapShaderModule,
                entryPoint: 'fs',
                targets: [{
                    format: 'rgba8unorm',
                }],
            },
            primitive: {
                topology: 'triangle-list',
            },
        });
    }

    const encoder = device.createCommandEncoder();

    let currentWidth = width;
    let currentHeight = height;

    for (let i = 1; i < mipCount; i++) {
        const nextWidth = Math.max(1, currentWidth >> 1);
        const nextHeight = Math.max(1, currentHeight >> 1);

        const passEncoder = encoder.beginRenderPass({
            colorAttachments: [{
                view: texture.createView({
                    baseMipLevel: i,
                    mipLevelCount: 1,
                }),
                loadOp: 'clear',
                storeOp: 'store',
                clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
            }],
        });

        const bindGroup = device.createBindGroup({
            layout: mipmapPipeline.getBindGroupLayout(0),
            entries: [
                {
                    binding: 0,
                    resource: texture.createView({
                        baseMipLevel: i - 1,
                        mipLevelCount: 1,
                    }),
                },
                {
                    binding: 1,
                    resource: mipmapSampler,
                },
            ],
        });

        passEncoder.setPipeline(mipmapPipeline);
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.draw(3);
        passEncoder.end();

        currentWidth = nextWidth;
        currentHeight = nextHeight;
    }

    device.queue.submit([encoder.finish()]);
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
    editor.style.fontKerning = 'none';
    editor.style.fontVariantLigatures = 'none';
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
            
            const panText = `X: ${Math.round(pan_x)}, Y: ${Math.round(pan_y)}`;
            const zoomText = `${Math.round(zoom * 100)}%`;
            
            if (panText !== lastPanText) {
                document.getElementById('stat-pan').innerText = panText;
                lastPanText = panText;
            }
            if (zoomText !== lastZoomText) {
                document.getElementById('stat-zoom').innerText = zoomText;
                lastZoomText = zoomText;
            }
            if (node_count !== lastNodeCount) {
                document.getElementById('stat-nodes').innerText = node_count;
                lastNodeCount = node_count;
            }
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
        if (panel.style.display !== 'none') {
            panel.style.display = 'none';
        }
        lastSelectedIdx = -1;
        lastProps.selectedIdx = -1;
        return;
    }

    if (panel.style.display !== 'flex') {
        panel.style.display = 'flex';
    }

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

    const textR = wasmInstance.exports.get_node_text_r(selectedIdx);
    const textG = wasmInstance.exports.get_node_text_g(selectedIdx);
    const textB = wasmInstance.exports.get_node_text_b(selectedIdx);

    // Check if anything has changed
    const changed = (
        selectedIdx !== lastProps.selectedIdx ||
        w !== lastProps.w ||
        h !== lastProps.h ||
        type !== lastProps.type ||
        wasmFontSize !== lastProps.fontSize ||
        bgR !== lastProps.bgR || bgG !== lastProps.bgG || bgB !== lastProps.bgB || bgA !== lastProps.bgA ||
        borderR !== lastProps.borderR || borderG !== lastProps.borderG || borderB !== lastProps.borderB || borderA !== lastProps.borderA ||
        textR !== lastProps.textR || textG !== lastProps.textG || textB !== lastProps.textB
    );

    if (!changed) {
        lastSelectedIdx = selectedIdx;
        return;
    }

    // Save to cache
    lastProps = {
        selectedIdx, w, h, type, fontSize: wasmFontSize,
        bgR, bgG, bgB, bgA,
        borderR, borderG, borderB, borderA,
        textR, textG, textB
    };

    if (!isUpdatingControls) {
        isUpdatingControls = true;

        const sizeGroup = document.getElementById('size-group');
        const borderGroup = document.getElementById('border-group');
        const bgColorLabel = document.getElementById('bg-color-label');
        const fontGroup = document.getElementById('font-group');

        // Font size control visibility (WIDGET_STICKY, RECT, OVAL, TEXT can have text)
        if (type <= 3 || type === 7) {
            fontGroup.style.display = 'flex';
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
            sizeGroup.style.display = 'flex';
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
                borderGroup.style.display = 'flex';
            }
        }

        const hexBg = rgbToHex(bgR, bgG, bgB);
        document.getElementById('prop-bg-color').value = hexBg;
        document.getElementById('prop-bg-transparent').checked = (bgA <= 0.001);
        document.getElementById('prop-bg-color').disabled = (bgA <= 0.001);

        const bgOpacityPct = Math.round(bgA * 100);
        const bgOpacitySlider = document.getElementById('prop-bg-opacity');
        const bgOpacityVal = document.getElementById('prop-bg-opacity-val');
        if (bgOpacitySlider && document.activeElement !== bgOpacitySlider) {
            bgOpacitySlider.value = bgOpacityPct;
            bgOpacitySlider.disabled = (bgA <= 0.001);
        }
        if (bgOpacityVal) {
            bgOpacityVal.innerText = bgOpacityPct + "%";
        }

        const hexBorder = rgbToHex(borderR, borderG, borderB);
        document.getElementById('prop-border-color').value = hexBorder;
        document.getElementById('prop-border-transparent').checked = (borderA <= 0.001);
        document.getElementById('prop-border-color').disabled = (borderA <= 0.001);

        // Retrieve and set font color
        document.getElementById('prop-font-color').value = rgbToHex(textR, textG, textB);

        if (type <= 3 || type === 7) {
            document.getElementById('font-color-group').style.display = 'flex';
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

        const opacitySlider = document.getElementById('prop-bg-opacity');
        if (opacitySlider) {
            opacitySlider.disabled = isTransparent;
        }

        const hex = document.getElementById('prop-bg-color').value;
        const rgb = hexToRgb(hex);
        const type = wasmInstance.exports.get_node_type(idx);

        let a = 0.9;
        if (isTransparent) {
            a = 0.0;
        } else if (opacitySlider) {
            a = parseFloat(opacitySlider.value) / 100.0;
            // If the background was transparent and is now toggled to opaque,
            // we revert to a sensible default opacity (90% for shapes, 100% for lines)
            if (a <= 0.001) {
                a = ((type === 5 || type === 6) ? 1.0 : 0.9);
                opacitySlider.value = Math.round(a * 100);
                const bgOpacityVal = document.getElementById('prop-bg-opacity-val');
                if (bgOpacityVal) {
                    bgOpacityVal.innerText = opacitySlider.value + "%";
                }
            }
        } else {
            a = ((type === 5 || type === 6) ? 1.0 : 0.9);
        }

        wasmInstance.exports.set_node_bg_color(idx, rgb.r, rgb.g, rgb.b, a);
        updateEditorStyle();
    };

    document.getElementById('prop-bg-color').addEventListener('input', updateBg);
    document.getElementById('prop-bg-transparent').addEventListener('change', updateBg);

    const bgOpacitySlider = document.getElementById('prop-bg-opacity');
    if (bgOpacitySlider) {
        bgOpacitySlider.addEventListener('input', () => {
            const val = bgOpacitySlider.value;
            const bgOpacityVal = document.getElementById('prop-bg-opacity-val');
            if (bgOpacityVal) {
                bgOpacityVal.innerText = val + "%";
            }

            // Sync with transparent checkbox
            const bgTransparentCheckbox = document.getElementById('prop-bg-transparent');
            if (bgTransparentCheckbox) {
                if (val > 0 && bgTransparentCheckbox.checked) {
                    bgTransparentCheckbox.checked = false;
                } else if (val == 0 && !bgTransparentCheckbox.checked) {
                    bgTransparentCheckbox.checked = true;
                }
            }

            updateBg();
        });
    }

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
        const dpr = window.devicePixelRatio || 1.0;
        canvas.width = Math.round(width * dpr);
        canvas.height = Math.round(height * dpr);

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
        // Start Render Loop
        function step(timestamp) {
            frameCountForFps++;
            const now = performance.now();
            if (now - lastFpsUpdateTime >= 500) {
                currentFps = Math.round((frameCountForFps * 1000) / (now - lastFpsUpdateTime));
                frameCountForFps = 0;
                lastFpsUpdateTime = now;
            }

            instance.exports.tick_app(timestamp);
            updatePropertiesPanel();
            updateEditorStyle();

            if (isDebugExpanded && now - lastDebugPanelUpdateTime >= 200) {
                updateDebugStatsPanel();
                lastDebugPanelUpdateTime = now;
            }

            requestAnimationFrame(step);
        }
        requestAnimationFrame(step);

    } catch (err) {
        console.error(err);
        document.getElementById('loader').style.display = 'block';
        document.getElementById('error-msg').innerText = err.message;
    }
}

function updateDebugStatsPanel() {
    if (!wasmInstance) return;

    const ptr = wasmInstance.exports.get_widget_type_counts();
    const counts = new Int32Array(wasmMemory.buffer, ptr, 8);

    const stickyCount = counts[0];
    const rectCount = counts[1];
    const ovalCount = counts[2];
    const textCount = counts[3];
    const imageCount = counts[4];
    const pathCount = counts[5];
    const arrowCount = counts[6];
    const triangleCount = counts[7];

    const batchCount = wasmInstance.exports.get_debug_batch_count();
    const vertexCount = wasmInstance.exports.get_debug_vertex_count();
    const indexCount = wasmInstance.exports.get_debug_index_count();

    const wasmMemBytes = wasmMemory.buffer.byteLength;
    const wasmMemMb = (wasmMemBytes / (1024 * 1024)).toFixed(1);

    let jsMemInfo = "";
    if (performance && performance.memory) {
        const jsUsedMb = (performance.memory.usedJSHeapSize / (1024 * 1024)).toFixed(1);
        const jsTotalMb = (performance.memory.totalJSHeapSize / (1024 * 1024)).toFixed(1);
        jsMemInfo = `<div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>JS Heap:</span><span>${jsUsedMb} / ${jsTotalMb} MB</span></div>`;
    }

    const html = `
        <div style="display: grid; grid-template-columns: 1fr; gap: 8px; font-family: monospace; line-height: 1.4;">
            <div style="font-weight: 600; color: #8b94f6; margin-bottom: 2px; border-bottom: 1px solid rgba(255,255,255,0.05); padding-bottom: 2px;">PERFORMANCE & SYSTEM</div>
            <div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>FPS:</span><span style="color: ${currentFps >= 50 ? '#4ade80' : currentFps >= 30 ? '#fbbf24' : '#f87171'}; font-weight: bold;">${currentFps} FPS</span></div>
            <div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>WASM Memory:</span><span>${wasmMemMb} MB</span></div>
            ${jsMemInfo}
            
            <div style="font-weight: 600; color: #8b94f6; margin-top: 6px; margin-bottom: 2px; border-bottom: 1px solid rgba(255,255,255,0.05); padding-bottom: 2px;">RENDERER (WEBGPU)</div>
            <div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>Draw Batches:</span><span>${batchCount}</span></div>
            <div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>Vertices:</span><span>${vertexCount.toLocaleString()}</span></div>
            <div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>Indices:</span><span>${indexCount.toLocaleString()}</span></div>
            <div style="display: flex; justify-content: space-between; margin-bottom: 2px;"><span>Zoom:</span><span>${(currentZoom * 100).toFixed(1)}%</span></div>
            
            <div style="font-weight: 600; color: #8b94f6; margin-top: 6px; margin-bottom: 2px; border-bottom: 1px solid rgba(255,255,255,0.05); padding-bottom: 2px;">OBJECT BREAKDOWN</div>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 4px 12px; font-size: 0.7rem;">
                <div style="display: flex; justify-content: space-between;"><span>Sticky:</span><span>${stickyCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Rects:</span><span>${rectCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Ovals:</span><span>${ovalCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Texts:</span><span>${textCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Images:</span><span>${imageCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Paths:</span><span>${pathCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Arrows:</span><span>${arrowCount}</span></div>
                <div style="display: flex; justify-content: space-between;"><span>Triangles:</span><span>${triangleCount}</span></div>
            </div>
        </div>
    `;

    document.getElementById('debug-expanded-panel').innerHTML = html;
}

function setupInputHandlers() {
    const canvas = document.getElementById('canvas');

    // Prevent all default touch behaviors on the document to stop
    // browser gesture interception (pull-to-refresh, back swipe, etc.)
    document.addEventListener('touchmove', (e) => {
        if (e.target === canvas || e.target.closest('#bottom-ui-container')) {
            e.preventDefault();
        }
    }, { passive: false });

    // Prevent double-tap-to-zoom on the canvas
    document.addEventListener('touchstart', (e) => {
        if (e.target === canvas) {
            e.preventDefault();
        }
    }, { passive: false });

    // Prevent native page pinch zoom
    document.addEventListener('gesturestart', (e) => {
        e.preventDefault();
    });

    let isDrawMode = false;
    let isDrawingStroke = false;
    let isArrowMode = false;
    let lastClientX = window.innerWidth / 2;
    let lastClientY = window.innerHeight / 2;

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
        lastClientX = e.clientX;
        lastClientY = e.clientY;
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

    window.addEventListener('wheel', (e) => {
        e.preventDefault();
        const coords = getCanvasCoords(e);
        if (e.ctrlKey) {
            // Trackpad pinch-to-zoom gesture
            wasmInstance.exports.on_mouse_wheel(e.deltaY, coords.x, coords.y);
        } else {
            // Trackpad two-finger scroll panning
            wasmInstance.exports.pan_canvas(-e.deltaX, -e.deltaY);
        }
    }, { passive: false });

    // Touch screen state trackers
    let lastTouchX = 0;
    let lastTouchY = 0;
    let isTouchDrawing = false;

    // Two-finger touch trackers
    let lastTouchDist = 0;
    let lastTouchMidX = 0;
    let lastTouchMidY = 0;
    let isTwoFingerTouch = false;

    function getTouchCoords(touch) {
        const rect = canvas.getBoundingClientRect();
        return {
            x: touch.clientX - rect.left,
            y: touch.clientY - rect.top
        };
    }

    canvas.addEventListener('touchstart', (e) => {
        commitText();

        if (e.touches.length === 1) {
            isTwoFingerTouch = false;
            const touch = e.touches[0];
            const coords = getTouchCoords(touch);
            lastTouchX = coords.x;
            lastTouchY = coords.y;

            if (isDrawMode) {
                isTouchDrawing = true;
                const wCoords = getWorldCoords(touch.clientX, touch.clientY);
                const hex = document.getElementById('draw-color-picker').value || '#1accde';
                const rgb = hexToRgb(hex);
                wasmInstance.exports.start_stroke(wCoords.x, wCoords.y, rgb.r, rgb.g, rgb.b);
            } else {
                const forceShift = isArrowMode ? 1 : 0;
                wasmInstance.exports.on_mouse_down(4, coords.x, coords.y, forceShift, 0);
            }
        } else if (e.touches.length === 2) {
            isTwoFingerTouch = true;
            if (isTouchDrawing) {
                wasmInstance.exports.end_stroke();
                isTouchDrawing = false;
            }
            // Cancel any single-finger drag
            wasmInstance.exports.on_mouse_up(0, lastTouchX, lastTouchY);

            const t1 = e.touches[0];
            const t2 = e.touches[1];
            lastTouchDist = Math.hypot(t1.clientX - t2.clientX, t1.clientY - t2.clientY);
            lastTouchMidX = (t1.clientX + t2.clientX) / 2;
            lastTouchMidY = (t1.clientY + t2.clientY) / 2;
        }
        e.preventDefault();
    }, { passive: false });

    canvas.addEventListener('touchmove', (e) => {
        if (e.touches.length === 1 && !isTwoFingerTouch) {
            const touch = e.touches[0];
            const coords = getTouchCoords(touch);
            lastTouchX = coords.x;
            lastTouchY = coords.y;

            if (isTouchDrawing) {
                const wCoords = getWorldCoords(touch.clientX, touch.clientY);
                wasmInstance.exports.add_stroke_point(wCoords.x, wCoords.y);
            } else {
                wasmInstance.exports.on_mouse_move(coords.x, coords.y);
            }
        } else if (e.touches.length === 2) {
            const t1 = e.touches[0];
            const t2 = e.touches[1];
            const dist = Math.hypot(t1.clientX - t2.clientX, t1.clientY - t2.clientY);
            const midX = (t1.clientX + t2.clientX) / 2;
            const midY = (t1.clientY + t2.clientY) / 2;

            // Pan midpoint difference
            const dx = midX - lastTouchMidX;
            const dy = midY - lastTouchMidY;
            wasmInstance.exports.pan_canvas(dx, dy);

            // Zoom distance difference
            if (Math.abs(dist - lastTouchDist) > 1) {
                const deltaY = (lastTouchDist - dist) * 1.5;
                const rect = canvas.getBoundingClientRect();
                const canvasMidX = midX - rect.left;
                const canvasMidY = midY - rect.top;
                wasmInstance.exports.on_mouse_wheel(deltaY, canvasMidX, canvasMidY);
            }

            lastTouchDist = dist;
            lastTouchMidX = midX;
            lastTouchMidY = midY;
        }
        e.preventDefault();
    }, { passive: false });

    canvas.addEventListener('touchend', (e) => {
        if (isTouchDrawing) {
            wasmInstance.exports.end_stroke();
            isTouchDrawing = false;
        } else if (!isTwoFingerTouch) {
            wasmInstance.exports.on_mouse_up(0, lastTouchX, lastTouchY);
        }

        if (e.touches.length === 0) {
            isTwoFingerTouch = false;
        } else if (e.touches.length === 1) {
            // Degrade to single touch pan starting point
            isTwoFingerTouch = false;
            const touch = e.touches[0];
            const coords = getTouchCoords(touch);
            lastTouchX = coords.x;
            lastTouchY = coords.y;
        }
        e.preventDefault();
    }, { passive: false });

    canvas.addEventListener('touchcancel', (e) => {
        if (isTouchDrawing) {
            wasmInstance.exports.end_stroke();
            isTouchDrawing = false;
        } else {
            wasmInstance.exports.on_mouse_up(0, lastTouchX, lastTouchY);
        }
        isTwoFingerTouch = false;
    }, { passive: false });

    canvas.addEventListener('contextmenu', (e) => {
        e.preventDefault();
    });

    window.addEventListener('resize', () => {
        const w = window.innerWidth;
        const h = window.innerHeight;
        const dpr = window.devicePixelRatio || 1.0;
        canvas.width = Math.round(w * dpr);
        canvas.height = Math.round(h * dpr);
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

    window.addEventListener('copy', (e) => {
        if (document.activeElement === editor) {
            return;
        }
        const idx = wasmInstance.exports.get_selected_node_idx();
        if (idx !== -1) {
            const textPtr = wasmInstance.exports.get_node_text_ptr(idx);
            const textContent = readNullTerminatedString(textPtr);
            const data = {
                _isDustNode: true,
                type: wasmInstance.exports.get_node_type(idx),
                w: wasmInstance.exports.get_node_width(idx),
                h: wasmInstance.exports.get_node_height(idx),
                bg_r: wasmInstance.exports.get_node_bg_r(idx),
                bg_g: wasmInstance.exports.get_node_bg_g(idx),
                bg_b: wasmInstance.exports.get_node_bg_b(idx),
                bg_a: wasmInstance.exports.get_node_bg_a(idx),
                border_r: wasmInstance.exports.get_node_border_r(idx),
                border_g: wasmInstance.exports.get_node_border_g(idx),
                border_b: wasmInstance.exports.get_node_border_b(idx),
                border_a: wasmInstance.exports.get_node_border_a(idx),
                text_r: wasmInstance.exports.get_node_text_r(idx),
                text_g: wasmInstance.exports.get_node_text_g(idx),
                text_b: wasmInstance.exports.get_node_text_b(idx),
                font_size: wasmInstance.exports.get_node_font_size(idx),
                texture_id: wasmInstance.exports.get_node_texture_id(idx),
                text: textContent
            };
            e.clipboardData.setData('text/plain', JSON.stringify(data));
            e.preventDefault();
        }
    });

    window.addEventListener('paste', async (e) => {
        if (document.activeElement === editor) {
            return;
        }

        // 1. Try pasting an image from clipboard
        const items = (e.clipboardData || e.originalEvent.clipboardData).items;
        for (let i = 0; i < items.length; i++) {
            if (items[i].type.indexOf('image') !== -1) {
                const file = items[i].getAsFile();
                await pasteImageFromClipboard(file);
                e.preventDefault();
                return;
            }
        }

        // 2. Try pasting serialized Dust node data or plain text
        const text = e.clipboardData.getData('text/plain');
        if (text) {
            try {
                const data = JSON.parse(text);
                if (data && data._isDustNode) {
                    pasteNode(data);
                    e.preventDefault();
                    return;
                }
            } catch (err) {
                // Not a Dust node JSON, fallback to text pasting
            }
            // Paste a text note with the clipboard content
            pasteTextNode(text);
            e.preventDefault();
        }
    });

    async function pasteImageFromClipboard(file) {
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

                const coords = getWorldCoords(lastClientX, lastClientY);
                // 4 is WIDGET_IMAGE
                wasmInstance.exports.add_widget_wasm(4, coords.x, coords.y, textureId, img.width, img.height);
            };
            img.src = event.target.result;
        };
        reader.readAsDataURL(file);
    }

    function pasteNode(data) {
        const coords = getWorldCoords(lastClientX, lastClientY);
        // Add slightly offset from cursor (or at cursor)
        wasmInstance.exports.add_widget_wasm(data.type, coords.x, coords.y, data.texture_id, data.w, data.h);
        
        const newIdx = wasmInstance.exports.get_selected_node_idx();
        if (newIdx !== -1) {
            wasmInstance.exports.set_node_size(newIdx, data.w, data.h);
            wasmInstance.exports.set_node_bg_color(newIdx, data.bg_r, data.bg_g, data.bg_b, data.bg_a);
            wasmInstance.exports.set_node_border_color(newIdx, data.border_r, data.border_g, data.border_b, data.border_a);
            wasmInstance.exports.set_node_text_color(newIdx, data.text_r, data.text_g, data.text_b);
            wasmInstance.exports.set_node_font_size(newIdx, data.font_size);
            if (data.text && data.text.length > 0) {
                const textPtr = wasmInstance.exports.get_node_text_ptr(newIdx);
                writeString(textPtr, 128, data.text);
            }
            wasmInstance.exports.mark_dirty_wasm();
        }
    }

    function pasteTextNode(text) {
        const coords = getWorldCoords(lastClientX, lastClientY);
        // 3 is WIDGET_TEXT, -1 for texture_id, 0 for size (will default)
        wasmInstance.exports.add_widget_wasm(3, coords.x, coords.y, -1, 0, 0);
        
        const newIdx = wasmInstance.exports.get_selected_node_idx();
        if (newIdx !== -1) {
            const textPtr = wasmInstance.exports.get_node_text_ptr(newIdx);
            writeString(textPtr, 128, text);
            // Default styling for pasted text: dark slate navy
            wasmInstance.exports.set_node_text_color(newIdx, 30/255, 41/255, 59/255);
            wasmInstance.exports.mark_dirty_wasm();
        }
    }

    document.getElementById('btn-bulk-create').addEventListener('click', () => {
        commitText();
        wasmInstance.exports.create_100k_infographics();
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

    document.getElementById('btn-add-triangle').addEventListener('click', () => {
        commitText();
        const center = getScreenCenterInWorld();
        wasmInstance.exports.add_widget_wasm(7, center.x, center.y, -1, 0, 0);
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
        try {
            wasmInstance.exports.on_btn_clear_click();
            console.log("on_btn_clear_click finished calling WASM");
        } catch (e) {
            console.error("WASM on_btn_clear_click crashed:", e);
        }
    });

    const btnToggleDebug = document.getElementById('btn-toggle-debug');
    const debugExpandedPanel = document.getElementById('debug-expanded-panel');
    btnToggleDebug.addEventListener('click', () => {
        isDebugExpanded = !isDebugExpanded;
        if (isDebugExpanded) {
            debugExpandedPanel.style.display = 'block';
            btnToggleDebug.style.background = 'rgba(94, 106, 210, 0.35)';
            btnToggleDebug.style.borderColor = 'rgba(94, 106, 210, 0.6)';
            updateDebugStatsPanel();
        } else {
            debugExpandedPanel.style.display = 'none';
            btnToggleDebug.style.background = 'rgba(94, 106, 210, 0.15)';
            btnToggleDebug.style.borderColor = 'rgba(94, 106, 210, 0.3)';
        }
    });
}

// Start execution
start();
