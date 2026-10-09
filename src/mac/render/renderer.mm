#include "renderer.h"

#include "bmp.h"
#include "game.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

/* Per-vertex draw mode, read by the fragment shader. */
enum Mode {
    MODE_SOLID = 0,
    MODE_CARD = 1,
    MODE_FELT = 2,
    MODE_TEXT_SMALL = 3,
    MODE_TEXT_LARGE = 4,
    MODE_CIRCLE = 5,
};

/* Texture-array slices: CARD1..CARD52, then CARDBACK and bitmap 108. */
constexpr int kSliceBack = 52;
constexpr int kSliceEmpty = 53;
constexpr int kSlices = 54;

struct Vertex {
    float x, y;
    float u, v;
    float r, g, b, a;
    float layer;
    float mode;
};

const char *kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct Vertex {
    float2 pos;
    float2 uv;
    packed_float4 color;
    float layer;
    float mode;
};

struct Out {
    float4 pos [[position]];
    float2 uv;
    float4 color;
    float layer [[flat]];
    float mode [[flat]];
};

vertex Out vs_main(const device Vertex *verts [[buffer(0)]],
                   constant float2 &view [[buffer(1)]],
                   uint vid [[vertex_id]]) {
    Vertex v = verts[vid];
    Out o;
    o.pos = float4(v.pos.x / view.x * 2.0 - 1.0, 1.0 - v.pos.y / view.y * 2.0, 0.0, 1.0);
    o.uv = v.uv;
    o.color = float4(v.color);
    o.layer = v.layer;
    o.mode = v.mode;
    return o;
}

fragment float4 fs_main(Out in [[stage_in]],
                        texture2d_array<float> cards [[texture(0)]],
                        texture2d<float> felt [[texture(1)]],
                        texture2d<float> small_text [[texture(2)]],
                        texture2d<float> large_text [[texture(3)]]) {
    constexpr sampler nearest(filter::nearest, address::clamp_to_edge);
    constexpr sampler smooth(filter::linear, address::clamp_to_edge);
    int mode = int(in.mode + 0.5);
    if (mode == 1) {
        return cards.sample(nearest, in.uv, uint(in.layer + 0.5)) * in.color;
    }
    if (mode == 2) {
        /* draw_felt tiles 63x64 cells of the 64x64 FELT bitmap. */
        uint2 t = uint2(uint(fmod(floor(in.uv.x), 63.0)), uint(fmod(floor(in.uv.y), 64.0)));
        return felt.read(t);
    }
    if (mode == 3) {
        return float4(in.color.rgb, in.color.a * small_text.sample(smooth, in.uv).r);
    }
    if (mode == 4) {
        return float4(in.color.rgb, in.color.a * large_text.sample(smooth, in.uv).r);
    }
    if (mode == 5) {
        /* Ellipse with the DC's default 1px black pen. uv is -1..1; layer is
         * the pen width as a fraction of the radius. */
        float d = length(in.uv);
        if (d > 1.0) {
            discard_fragment();
        }
        return d > 1.0 - in.layer ? float4(0.0, 0.0, 0.0, 1.0) : in.color;
    }
    return in.color;
}
)";

struct Glyph {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    float w = 0, h = 0; /* quad size, client units */
    float ox = 0, oy = 0; /* quad origin relative to pen and baseline */
    float advance = 0;
};

struct Atlas {
    id<MTLTexture> tex = nil;
    Glyph glyphs[96];
    float ascent = 0;
    float scale = 1;
};

}  // namespace

struct Renderer::Impl {
    SDL_Window *window = nullptr;
    SDL_MetalView view = nullptr;
    CAMetalLayer *layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLRenderPipelineState> blend_pso = nil;
    id<MTLRenderPipelineState> invert_pso = nil;
    id<MTLTexture> cards = nil;
    id<MTLTexture> felt = nil;
    Atlas small_font;
    Atlas large_font;
    float atlas_scale = 0;
    id<CAMetalDrawable> drawable = nil;
    int backing_w = 0;
    int backing_h = 0;
    std::vector<Vertex> verts;
    size_t base_end = 0;
    size_t invert_end = 0;

    void ensure_fonts(float scale);
    void build(const Frame &frame, float w, float h);
    void encode(id<MTLCommandBuffer> cmd, id<MTLTexture> target, float w, float h);
    bool build_pipelines();
    bool load_textures(const std::string &dir);
    bool build_atlas(Atlas &atlas, NSString *name, NSString *fallback, CGFloat size, float scale);
    void quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float b,
              float a, float layer, int mode);
    void solid(float x, float y, float w, float h, float r, float g, float b, float a = 1);
    void card(const Sprite &s);
    float text_width(const Atlas &atlas, const char *text) const;
    void text(const Atlas &atlas, int mode, float x, float top, const char *text, float r, float g, float b);
};

Renderer::Renderer() : impl(new Impl) {}

Renderer::~Renderer() { shutdown(); }

bool Renderer::Impl::build_pipelines() {
    NSError *err = nil;
    id<MTLLibrary> lib = [device newLibraryWithSource:@(kShaderSource) options:nil error:&err];
    if (lib == nil) {
        SDL_Log("metal: shader compile failed: %s", err.localizedDescription.UTF8String);
        return false;
    }
    MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = [lib newFunctionWithName:@"vs_main"];
    desc.fragmentFunction = [lib newFunctionWithName:@"fs_main"];
    MTLRenderPipelineColorAttachmentDescriptor *ca = desc.colorAttachments[0];
    ca.pixelFormat = layer.pixelFormat;
    ca.blendingEnabled = YES;
    ca.rgbBlendOperation = MTLBlendOperationAdd;
    ca.alphaBlendOperation = MTLBlendOperationAdd;
    ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
    ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    blend_pso = [device newRenderPipelineStateWithDescriptor:desc error:&err];
    if (blend_pso == nil) {
        SDL_Log("metal: pipeline: %s", err.localizedDescription.UTF8String);
        return false;
    }
    /* InvertRect: dst = 1 - dst, drawn with a white source. */
    ca.sourceRGBBlendFactor = MTLBlendFactorOneMinusDestinationColor;
    ca.destinationRGBBlendFactor = MTLBlendFactorZero;
    ca.sourceAlphaBlendFactor = MTLBlendFactorZero;
    ca.destinationAlphaBlendFactor = MTLBlendFactorOne;
    invert_pso = [device newRenderPipelineStateWithDescriptor:desc error:&err];
    if (invert_pso == nil) {
        SDL_Log("metal: invert pipeline: %s", err.localizedDescription.UTF8String);
        return false;
    }
    return true;
}

bool Renderer::Impl::load_textures(const std::string &dir) {
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.textureType = MTLTextureType2DArray;
    desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    desc.width = kCardW;
    desc.height = kCardH;
    desc.arrayLength = kSlices;
    desc.usage = MTLTextureUsageShaderRead;
    cards = [device newTextureWithDescriptor:desc];

    for (int slice = 0; slice < kSlices; slice++) {
        std::string name;
        if (slice < 52) {
            name = "CARD" + std::to_string(slice + 1);
        } else if (slice == kSliceBack) {
            name = "CARDBACK";
        } else {
            name = "108";
        }
        Image img;
        std::string path = dir + "/bitmaps/" + name + ".bmp";
        if (!load_bmp(path, img) || img.w != kCardW || img.h != kCardH) {
            SDL_Log("assets: cannot load %s", path.c_str());
            return false;
        }
        int code = slice + 1;
        if ((code >= 0xe && code <= 0x17) || (code >= 0x1b && code <= 0x24)) {
            stroke_octagon(img);
        }
        punch_corners(img);
        [cards replaceRegion:MTLRegionMake2D(0, 0, kCardW, kCardH)
                 mipmapLevel:0
                       slice:slice
                   withBytes:img.rgba.data()
                 bytesPerRow:kCardW * 4
               bytesPerImage:kCardW * kCardH * 4];
    }

    Image tile;
    std::string felt_path = dir + "/bitmaps/FELT.bmp";
    if (!load_bmp(felt_path, tile)) {
        SDL_Log("assets: cannot load %s", felt_path.c_str());
        return false;
    }
    MTLTextureDescriptor *fd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                  width:tile.w
                                                                                 height:tile.h
                                                                              mipmapped:NO];
    fd.usage = MTLTextureUsageShaderRead;
    felt = [device newTextureWithDescriptor:fd];
    [felt replaceRegion:MTLRegionMake2D(0, 0, tile.w, tile.h)
            mipmapLevel:0
              withBytes:tile.rgba.data()
            bytesPerRow:tile.w * 4];
    return true;
}

/* Rasterize ASCII 32-126 with CoreText into one 8-bit coverage texture at the
 * drawable's pixel scale. */
bool Renderer::Impl::build_atlas(Atlas &atlas, NSString *name, NSString *fallback, CGFloat size, float scale) {
    CGFloat px = size * scale;
    CTFontRef font = CTFontCreateWithName((__bridge CFStringRef)name, px, nullptr);
    NSString *got = CFBridgingRelease(CTFontCopyPostScriptName(font));
    if (![got isEqualToString:name] && fallback != nil) {
        CFRelease(font);
        font = CTFontCreateWithName((__bridge CFStringRef)fallback, px, nullptr);
    }
    CGFloat ascent = CTFontGetAscent(font);
    CGFloat descent = CTFontGetDescent(font);
    UniChar chars[95];
    CGGlyph glyphs[95];
    for (int i = 0; i < 95; i++) {
        chars[i] = (UniChar)(32 + i);
    }
    CTFontGetGlyphsForCharacters(font, chars, glyphs, 95);
    CGSize advances[95];
    CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, glyphs, advances, 95);
    CGRect bounds[95];
    CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal, glyphs, bounds, 95);

    const int pad = 2;
    CGFloat max_w = 0;
    for (int i = 0; i < 95; i++) {
        max_w = std::max(max_w, CGRectGetMaxX(bounds[i]) - std::min<CGFloat>(0, bounds[i].origin.x));
    }
    int cell_w = (int)std::ceil(max_w) + pad * 2;
    int cell_h = (int)std::ceil(ascent + descent) + pad * 2;
    const int cols = 16;
    const int rows = 6;
    int tw = cell_w * cols;
    int th = cell_h * rows;
    std::vector<uint8_t> pixels((size_t)tw * th, 0);
    CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), tw, th, 8, tw, gray, (CGBitmapInfo)kCGImageAlphaNone);
    CGColorSpaceRelease(gray);
    CGContextSetGrayFillColor(ctx, 1.0, 1.0);
    CGContextSetShouldAntialias(ctx, true);

    for (int i = 0; i < 95; i++) {
        int col = i % cols;
        int row = i / cols;
        CGFloat left = std::min<CGFloat>(0, bounds[i].origin.x);
        CGFloat pen_x = col * cell_w + pad - left;
        CGFloat base_row = row * cell_h + pad + ascent;
        CGPoint pos = CGPointMake(pen_x, th - base_row);
        CTFontDrawGlyphs(font, &glyphs[i], &pos, 1, ctx);
        Glyph &g = atlas.glyphs[i];
        g.u0 = (float)(col * cell_w) / tw;
        g.v0 = (float)(row * cell_h) / th;
        g.u1 = (float)((col + 1) * cell_w) / tw;
        g.v1 = (float)((row + 1) * cell_h) / th;
        g.w = cell_w / scale;
        g.h = cell_h / scale;
        g.ox = (float)(-pad + left) / scale;
        g.oy = (float)(-pad - ascent) / scale;
        g.advance = (float)advances[i].width / scale;
    }
    CGContextRelease(ctx);
    CFRelease(font);

    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                    width:tw
                                                                                   height:th
                                                                                mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    atlas.tex = [device newTextureWithDescriptor:desc];
    [atlas.tex replaceRegion:MTLRegionMake2D(0, 0, tw, th) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:tw];
    atlas.ascent = (float)ascent / scale;
    atlas.scale = scale;
    return true;
}

bool Renderer::init(SDL_Window *window, const std::string &asset_dir) {
    Impl &r = *impl;
    r.window = window;
    r.view = SDL_Metal_CreateView(window);
    if (r.view == nullptr) {
        SDL_Log("metal: %s", SDL_GetError());
        return false;
    }
    r.layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(r.view);
    r.device = MTLCreateSystemDefaultDevice();
    if (r.device == nil) {
        SDL_Log("metal: no device");
        return false;
    }
    r.layer.device = r.device;
    r.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    r.layer.framebufferOnly = YES;
    r.layer.displaySyncEnabled = YES;
    /* Two drawables, presented in the current Core Animation transaction.
     * The default queue of three, presented asynchronously, shows the cursor
     * a frame or two behind where GDI's BitBlt put it. */
    r.layer.maximumDrawableCount = 2;
    r.layer.presentsWithTransaction = YES;
    r.queue = [r.device newCommandQueue];
    return r.build_pipelines() && r.load_textures(asset_dir);
}

void Renderer::shutdown() {
    if (!impl) {
        return;
    }
    Impl &r = *impl;
    r.queue = nil;
    r.blend_pso = nil;
    r.invert_pso = nil;
    r.cards = nil;
    r.felt = nil;
    r.small_font.tex = nil;
    r.large_font.tex = nil;
    r.layer = nil;
    r.device = nil;
    if (r.view != nullptr) {
        SDL_Metal_DestroyView(r.view);
        r.view = nullptr;
    }
}

void Renderer::Impl::quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, float r,
                          float g, float b, float a, float lyr, int mode) {
    Vertex tl = {x, y, u0, v0, r, g, b, a, lyr, (float)mode};
    Vertex tr = {x + w, y, u1, v0, r, g, b, a, lyr, (float)mode};
    Vertex bl = {x, y + h, u0, v1, r, g, b, a, lyr, (float)mode};
    Vertex br = {x + w, y + h, u1, v1, r, g, b, a, lyr, (float)mode};
    verts.insert(verts.end(), {tl, tr, bl, tr, br, bl});
}

void Renderer::Impl::solid(float x, float y, float w, float h, float r, float g, float b, float a) {
    quad(x, y, w, h, 0, 0, 0, 0, r, g, b, a, 0, MODE_SOLID);
}

void Renderer::Impl::card(const Sprite &s) {
    int slice;
    if (s.code >= 1 && s.code <= 52) {
        slice = s.code - 1;
    } else if (s.code == CODE_BACK) {
        slice = kSliceBack;
    } else if (s.code == CODE_EMPTY) {
        slice = kSliceEmpty;
    } else {
        return;
    }
    quad(s.x, s.y, kCardW, kCardH, 0, 0, 1, 1, 1, 1, 1, 1, (float)slice, MODE_CARD);
}

float Renderer::Impl::text_width(const Atlas &atlas, const char *s) const {
    float w = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c >= 32 && c < 127) {
            w += atlas.glyphs[c - 32].advance;
        }
    }
    return w;
}

void Renderer::Impl::text(const Atlas &atlas, int mode, float x, float top, const char *s, float r, float g,
                          float b) {
    float baseline = top + atlas.ascent;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c < 32 || c >= 127) {
            continue;
        }
        const Glyph &gl = atlas.glyphs[c - 32];
        if (c != ' ') {
            quad(x + gl.ox, baseline + gl.oy, gl.w, gl.h, gl.u0, gl.v0, gl.u1, gl.v1, r, g, b, 1, 0, mode);
        }
        x += gl.advance;
    }
}

void Renderer::Impl::ensure_fonts(float scale) {
    if (scale == atlas_scale) {
        return;
    }
    /* paint_board's DC font, and reset_draw's 48pt Arial at weight 800. */
    build_atlas(small_font, @"Tahoma-Bold", @"Helvetica-Bold", 11, scale);
    build_atlas(large_font, @"Arial-BoldMT", @"Helvetica-Bold", 64, scale);
    atlas_scale = scale;
}

/* Paint order follows paint_hdc: felt, columns, score box, stock and
 * completed suits, then the moving cards. The hint is an inversion of what
 * is already drawn; the fireworks and text go on top. */
void Renderer::Impl::build(const Frame &frame, float w, float h) {
    verts.clear();
    quad(0, 0, w, h, 0, 0, w, h, 1, 1, 1, 1, 0, MODE_FELT);
    for (const Sprite &s : frame.board) {
        card(s);
    }
    if (frame.show_score) {
        /* paint_board: RGB(0,127,0) fill, black frame, white labels. */
        float x = (float)frame.score_x;
        float y = (float)frame.score_y;
        float bw = (float)frame.score_w;
        float bh = (float)frame.score_h;
        solid(x, y, bw, bh, 0, 127.0f / 255.0f, 0);
        solid(x, y, bw, 1, 0, 0, 0);
        solid(x, y + bh - 1, bw, 1, 0, 0, 0);
        solid(x, y, 1, bh, 0, 0, 0);
        solid(x + bw - 1, y, 1, bh, 0, 0, 0);
        const char *labels[2] = {"Score:", "Moves:"};
        float tops[2] = {y + 0x1e, y + 0x32};
        std::string values[2] = {std::to_string(frame.score), std::to_string(frame.moves)};
        for (int i = 0; i < 2; i++) {
            float lw = text_width(small_font, labels[i]);
            text(small_font, MODE_TEXT_SMALL, x + 100 - lw, tops[i], labels[i], 1, 1, 1);
            text(small_font, MODE_TEXT_SMALL, x + 0x6e, tops[i], values[i].c_str(), 1, 1, 1);
        }
    }
    for (const Sprite &s : frame.front) {
        card(s);
    }
    base_end = verts.size();

    if (frame.hint_on) {
        solid((float)frame.hint_x, (float)frame.hint_y, (float)frame.hint_w, (float)frame.hint_h, 1, 1, 1);
    }
    invert_end = verts.size();

    /* AnimState::paint draws the text (prep_blit) before the particles (run_fx / tick_fx). */
    if (frame.win_text) {
        const char *msg = "You Won!";
        float tw = text_width(large_font, msg);
        float th = large_font.glyphs[0].h - 4.0f / large_font.scale;
        text(large_font, MODE_TEXT_LARGE, std::floor((w - tw) / 2), std::floor((h - th) / 2), msg, frame.win_r,
             frame.win_g, frame.win_b);
    }
    for (const Particle &p : frame.fx) {
        float d = p.rad * 2;
        quad(p.x - p.rad, p.y - p.rad, d, d, -1, -1, 1, 1, p.r, p.g, p.b, 1, 1.0f / std::max(1.0f, p.rad),
             MODE_CIRCLE);
    }
}

void Renderer::Impl::encode(id<MTLCommandBuffer> cmd, id<MTLTexture> target, float w, float h) {
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0.5, 0, 1);

    id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];
    float view[2] = {w, h};
    id<MTLBuffer> vb = nil;
    if (!verts.empty()) {
        vb = [device newBufferWithBytes:verts.data()
                                 length:verts.size() * sizeof(Vertex)
                                options:MTLResourceStorageModeShared];
    }
    [enc setViewport:(MTLViewport){0, 0, (double)target.width, (double)target.height, 0, 1}];
    [enc setVertexBuffer:vb offset:0 atIndex:0];
    [enc setVertexBytes:view length:sizeof(view) atIndex:1];
    [enc setFragmentTexture:cards atIndex:0];
    [enc setFragmentTexture:felt atIndex:1];
    [enc setFragmentTexture:small_font.tex atIndex:2];
    [enc setFragmentTexture:large_font.tex atIndex:3];

    auto range = [&](id<MTLRenderPipelineState> pso, size_t from, size_t to) {
        if (to > from) {
            [enc setRenderPipelineState:pso];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:from vertexCount:to - from];
        }
    };
    range(blend_pso, 0, base_end);
    range(invert_pso, base_end, invert_end);
    range(blend_pso, invert_end, verts.size());
    [enc endEncoding];
}

void Renderer::acquire(int client_w, int client_h) {
    Impl &r = *impl;
    if (r.layer == nil || client_w <= 0 || client_h <= 0 || r.drawable != nil) {
        return;
    }
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(r.window, &pw, &ph);
    if (pw <= 0 || ph <= 0) {
        return;
    }
    if (pw != r.backing_w || ph != r.backing_h) {
        r.layer.drawableSize = CGSizeMake(pw, ph);
        r.backing_w = pw;
        r.backing_h = ph;
        r.ensure_fonts((float)pw / (float)client_w);
    }
    @autoreleasepool {
        r.drawable = [r.layer nextDrawable];
    }
}

void Renderer::draw(const Frame &frame, int client_w, int client_h) {
    Impl &r = *impl;
    if (r.layer == nil || client_w <= 0 || client_h <= 0) {
        return;
    }
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(r.window, &pw, &ph);
    if (r.drawable != nil && (r.drawable.texture.width != (NSUInteger)pw || r.drawable.texture.height != (NSUInteger)ph)) {
        r.drawable = nil;
    }
    acquire(client_w, client_h);
    if (r.drawable == nil) {
        return;
    }
    @autoreleasepool {
        id<CAMetalDrawable> drawable = r.drawable;
        r.drawable = nil;
        r.build(frame, (float)client_w, (float)client_h);
        id<MTLCommandBuffer> cmd = [r.queue commandBuffer];
        r.encode(cmd, drawable.texture, (float)client_w, (float)client_h);
        /* presentsWithTransaction forbids presentDrawable:. Waiting until the
         * buffer is scheduled, then presenting and flushing, puts this frame
         * on the next vsync instead of the one after it. */
        [cmd commit];
        [cmd waitUntilScheduled];
        [drawable present];
        [CATransaction flush];
    }
}

bool Renderer::snapshot(const Frame &frame, int client_w, int client_h, float scale, const std::string &png_path) {
    Impl &r = *impl;
    int pw = (int)std::lround(client_w * scale);
    int ph = (int)std::lround(client_h * scale);
    r.ensure_fonts(scale);
    @autoreleasepool {
        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:r.layer.pixelFormat
                                                                                        width:pw
                                                                                       height:ph
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModeShared;
        id<MTLTexture> target = [r.device newTextureWithDescriptor:desc];
        r.build(frame, (float)client_w, (float)client_h);
        id<MTLCommandBuffer> cmd = [r.queue commandBuffer];
        r.encode(cmd, target, (float)client_w, (float)client_h);
        [cmd commit];
        [cmd waitUntilCompleted];

        std::vector<uint8_t> pixels((size_t)pw * ph * 4);
        [target getBytes:pixels.data() bytesPerRow:pw * 4 fromRegion:MTLRegionMake2D(0, 0, pw, ph) mipmapLevel:0];
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(pixels.data(), pw, ph, 8, pw * 4, rgb,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGImageRef image = CGBitmapContextCreateImage(ctx);
        NSURL *url = [NSURL fileURLWithPath:@(png_path.c_str())];
        CGImageDestinationRef dst =
            CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
        bool ok = false;
        if (dst != nullptr) {
            CGImageDestinationAddImage(dst, image, nullptr);
            ok = CGImageDestinationFinalize(dst);
            CFRelease(dst);
        }
        CGImageRelease(image);
        CGContextRelease(ctx);
        CGColorSpaceRelease(rgb);
        return ok;
    }
}

