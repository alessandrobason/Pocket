#include "std/logging.h"
#include "std/arena.h"
#include "std/filesystem.h"

#include "gfx/engine.h"

#include <iostream>
#include <windows.h>

Engine *g_engine = nullptr;

#include "std/file.h"
#include "std/asio.h"
#include <chrono>

using timer = std::chrono::high_resolution_clock;
usize ms(auto t) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

int main(int argc, char* argv[]) {
	Arena log_arena = Arena::make(gb(1));
	trace::init(log_arena);

	const char *asset_folder = "assets";
	if (argc > 1) {
		asset_folder = argv[1];
	}
	else {
		info("no asset folder provided, using default");
	}

	info("asset folder: (%s)", asset_folder);

	fs::setBaseFolder(asset_folder);

	Engine engine;
	g_engine = &engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();

	return 0;
}
