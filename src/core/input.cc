#include "input.h"

#include <string.h>
#include <SDL.h>

#include "std/common.h"
#include "std/maths.h"

static bool prev_keys[(int)Key::__count] = {};
static bool keys[(int)Key::__count] = {};
static bool prev_mouse_btns[(int)Mouse::__count] = {};
static bool mouse_btns[(int)Mouse::__count] = {};
static vec2i mouse_pos = 0;
static vec2i prev_mouse_pos = 0;

static Key sdl__to_key(SDL_Keycode code);
static Mouse sdl__to_mouse(u8 button);

bool isKeyDown(Key key) {
    return keys[(int)key];
}

bool isKeyUp(Key key) {
    return !keys[(int)key];    
}

bool isKeyPressed(Key key) {
    return keys[(int)key] != prev_keys[(int)key];
}

bool isMouseDown(Mouse btn) {
    return mouse_btns[(int)btn];
}

bool isMouseUp(Mouse btn) {
    return !mouse_btns[(int)btn];
}

bool isMousePressed(Mouse btn) {
    return mouse_btns[(int)btn] != prev_mouse_btns[(int)btn];
}

vec2i getMousePos() {
    return mouse_pos;
}

vec2i getMouseRel() {
    return prev_mouse_pos - mouse_pos;
}

#include "std/logging.h"

void inputNewFrame() {
    memcpy(prev_keys, keys, sizeof(keys));
    memcpy(prev_mouse_btns, mouse_btns, sizeof(mouse_btns));
    prev_mouse_pos = mouse_pos;
}

void inputEvent(const union SDL_Event &event) {
    switch (event.type){
        /* Look for a keypress */
        case SDL_KEYDOWN:
            keys[(int)sdl__to_key(event.key.keysym.sym)] = true;
            break;
        case SDL_KEYUP:
            keys[(int)sdl__to_key(event.key.keysym.sym)] = false;
            break;
        case SDL_MOUSEBUTTONDOWN:
            mouse_btns[(int)sdl__to_mouse(event.button.button)] = true;
            break;
        case SDL_MOUSEBUTTONUP:
            mouse_btns[(int)sdl__to_mouse(event.button.button)] = false;
            break;
        case SDL_MOUSEMOTION:
            mouse_pos = { event.motion.x, event.motion.y };
            break;
    }
}

static Key sdl__to_key(SDL_Keycode code) {
    switch (code) {
        case SDLK_w: return Key::W;        
        case SDLK_a: return Key::A;        
        case SDLK_s: return Key::S;        
        case SDLK_d: return Key::D;
        case SDLK_LSHIFT: // fallthrough
        case SDLK_RSHIFT: return Key::Shift;
        case SDLK_LCTRL: // fallthrough
        case SDLK_RCTRL: return Key::Ctrl;
        case SDLK_ESCAPE: return Key::Escape;
    }

    return Key::None;
}

static Mouse sdl__to_mouse(u8 button) {
    return (Mouse)math::min(button, (u8)((int)Mouse::__count - 1));
}