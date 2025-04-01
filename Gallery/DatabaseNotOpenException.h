#pragma once
#include "MyException.h"

class DatabaseNotOpenException : public MyException
{
public:
	DatabaseNotOpenException() : MyException("Error: Can't perform operation, you need to open the database first") {}
};
