#pragma once

#include "state/datastate.hpp"
#include "gl/GLLoader.hpp"
#include <string>

// Owns every GL object used to turn decoded video frames into upscaled
// pixels on screen. Three passes per frame:
//   A. decode:   YUV420P (or sws_scale-converted BGRA) source -> RGB, into an
//                FBO texture at the source frame's native resolution.
//   B. upscale:  the active upscaler (FSR1 EASU, or NIS - see
//                cycle_upscaler()) into an FBO texture sized to the
//                destination (letterboxed) rect. Skipped entirely in
//                Bilinear mode.
//   C. composite: FSR1's RCAS sharpen (or, for NIS/Bilinear, a forced
//                passthrough reusing the same RCAS program - NIS already
//                bakes its own sharpen in), drawn straight to the backbuffer
//                at the destination rect.
// One video output per process, so (like `window`/`renderer` used to be)
// this is deliberately a set of static globals rather than an instance.
class VideoRenderer
{
    public:
        // Call once, after Window::create_window() has made a GL context
        // current. Compiles shaders and allocates the quad VBO/VAO.
        static bool init();
        static void destroy();

        // Uploads a decoded frame to GL textures, converting on the CPU via
        // sws_scale first for any pixel format that isn't planar YUV420P.
        static bool upload_frame(AVFrame *frame, struct SwsContext **img_convert_ctx);

        // Runs the decode -> upscale -> composite pipeline for the most
        // recently uploaded frame, landing the final image in `rect` within
        // a `drawable_w`x`drawable_h` viewport. `flip_v` mirrors the
        // vertical flip SDL_RenderCopyEx used to apply for bottom-up frames.
        static void draw(const SDL_Rect &rect, int drawable_w, int drawable_h, bool flip_v);

        static void clear();
        static void present();

        // FSR1 RCAS sharpening strength in AMD's own "stops" convention
        // (FsrRcasCon): 0.0 = maximum sharpness, each +1.0 halves it
        // (internally, lobe *= exp2(-sharpness)). Only applies in FSR1 mode.
        static void set_sharpness(float sharpness);

        // Cycles the active upscaler: Bilinear (off - plain GL_LINEAR
        // stretch, the same quality the old SDL_Renderer path had) -> FSR1
        // (EASU+RCAS) -> NIS -> back to Bilinear. Use for A/B comparison.
        static void cycle_upscaler();
        static const char *upscaler_name();

        // Cycles through AMD's published FSR1 quality presets (Native,
        // Ultra Quality 1.3x, Quality 1.5x, Balanced 1.7x, Performance 2.0x).
        // These control how much resolution the upscaler is asked to
        // reconstruct: the decoded frame is downsampled by the preset's
        // ratio before upscaling it back to display size, same as choosing
        // a quality mode in a game's FSR/NIS setting. Only affects anything
        // while an upscaler is active - see cycle_upscaler(). Note NIS's own
        // reference implementation only validates up to 2x per axis
        // (Performance); FSR1 documents no such limit.
        static void cycle_render_scale();
        static const char *render_scale_name();

        // Shows a short toast (translucent box + text, top-left corner) for
        // about a second before fading out over ~0.4s. Call again to replace
        // whatever's currently showing/fading. Text outside printable ASCII
        // renders as spaces.
        static void show_overlay(const std::string &text);

        // Toggles a persistent (not timed) keyboard-shortcut reference panel,
        // top-left corner, listing everything bound in Event.cpp. Unlike
        // show_overlay(), stays up until toggled off again rather than
        // fading, and can be shown at the same time as a toast.
        static void toggle_help();
        static bool help_visible();

    private:
        static bool ensure_yuv_textures(int width, int height);
        static bool ensure_rgba_texture(int width, int height);
        static bool ensure_decode_target(int width, int height);
        static bool ensure_upscale_target(int width, int height);
        static bool ensure_downscale_target(int width, int height);
        static void set_composite_quad(const SDL_Rect &rect, int drawable_w, int drawable_h);
        static void build_overlay_geometry();
        static void draw_overlay(int drawable_w, int drawable_h);
        static void build_help_geometry();
        static void draw_help(int drawable_w, int drawable_h);
};
