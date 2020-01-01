// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "DefaultFrame.h"

#if defined(NO_UE4) || defined(_CONSOLE)

struct End
{
	~End()
	{
		_CrtDumpMemoryLeaks();
	}
}end;

int main()
{

	{

		DefaultFrame frame;

		frame.init();
		frame.update();
	}

	return 0;
}

#endif