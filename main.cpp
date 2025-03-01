#include "App.hpp"

#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
#include <emscripten.h>
#endif

#include <iostream>

int main (int , char**)
{
	App app;
	if (!app.IsInitialized())
	{
		std::cerr << "App could not be Initialized. Exiting..." << std::endl;
		return 1;
	}

#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
	emscripten_set_main_loop_arg([](void* arg) {
			App* app = static_cast<App*>(arg);
			app->Tick();
	}, &app, 0, true);
#else
	while (app.IsRunning())
		app.Tick();
#endif

	app.Terminate();

	return 0;
}
