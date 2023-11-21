#pragma once

#include "std/vec.h"

enum class Key {
    None,
    W, A, S, D,
    Q, E,
    Shift, Ctrl,
    Escape,

    __count
};

enum class Mouse {
    None, 
    Left, 
    Right, 
    Middle,

    __count
};

bool isKeyDown(Key key);
bool isKeyUp(Key key);
bool isKeyPressed(Key key);

bool isMouseDown(Mouse btn = Mouse::Left);
bool isMouseUp(Mouse btn = Mouse::Left);
bool isMousePressed(Mouse btn = Mouse::Left);

vec2i getMousePos();
vec2i getMouseRel();

void inputNewFrame();
void inputEvent(const union SDL_Event &event);
