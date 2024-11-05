#include <iostream>

class InvalidScaleException : public std::exception
{
public:
	const char* what() {
		return "Scale not found";
	}
};

class InvalidAddonFileException : public std::exception
{
public:
	const char* what()
	{
		return "Invalid addon file";
	}
};
