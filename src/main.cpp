#include <vk_engine.h>

#include "std/logging.h"
#include "std/arena.h"

int main(int argc, char* argv[]) {
	Arena log_arena = Arena::make(gb(1));
	trace::init(log_arena);

	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
