#include "std/logging.h"
#include "std/arena.h"
#include "std/filesystem.h"

#include "gfx/engine.h"

#include "core/thread_pool.h"

#include <iostream>
#include <windows.h>

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

	ThreadPool thr_pool;
	thr_pool.start(3);
	
	thr_pool.pushJob([](){ info("sleeping for 1s"); Sleep(1000); info("finished 1s"); });
	thr_pool.pushJob([](){ info("sleeping for 2s"); Sleep(2000); info("finished 2s"); });
	thr_pool.pushJob([](){ info("sleeping for 3s"); Sleep(3000); info("finished 3s"); });
	thr_pool.pushJob([](){ info("sleeping for 4s"); Sleep(4000); info("finished 4s"); });
	thr_pool.pushJob([](){ info("sleeping for 5s"); Sleep(5000); info("finished 5s"); });
	thr_pool.pushJob([](){ info("sleeping for 6s"); Sleep(6000); info("finished 6s"); });

	while (thr_pool.isBusy()) {
		Sleep(100);
	}
	thr_pool.stop();

	Engine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();

	return 0;
}
