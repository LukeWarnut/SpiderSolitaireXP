#pragma once

#include <memory>
#include <string>

struct SDL_Window;
struct Frame;

/* Draws a Frame with Metal into the SDL window's CAMetalLayer. Coordinates
 * are client points; the projection maps them onto the drawable, so Retina
 * scales the XP pixel grid by an integer factor. */
class Renderer {
public:
    Renderer();
    ~Renderer();
    bool init(SDL_Window *window, const std::string &asset_dir);
    void draw(const Frame &frame, int client_w, int client_h);
    /* Render one frame offscreen at `scale` pixels per point and write a PNG. */
    bool snapshot(const Frame &frame, int client_w, int client_h, float scale, const std::string &png_path);
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
