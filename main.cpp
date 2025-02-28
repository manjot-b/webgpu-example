#include <iostream>
#include "App.hpp"

int main (int , char**)
{
	App app;
	if (!app.Initialized())
	{
		std::cerr << "App could not be Initialized. Exiting..." << std::endl;
		return 1;
	}

	while (app.IsRunning())
	{
		app.Tick();
	}

	app.Terminate();

	return 0;
}
