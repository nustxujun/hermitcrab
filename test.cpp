// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#if defined(_EXAMPLE)

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
		try{
			frame.init(true);
			frame.update();
		}
		catch (...)
		{
			frame.rendercmd.invalid();
		}
	}

	return 0;
}

#endif