#pragma once
#include "MyException.h"
#include <string.h>

class FailedSQLQueryException : public MyException
{
public:
	FailedSQLQueryException(const std::string& message, const std::string& sqlQuery) :
		MyException(message), m_combinedMessage(message + "\nSQL QUERY: " + sqlQuery) {}

	virtual const char* what() const noexcept 
	{
		return m_combinedMessage.c_str();
	};

private:
	std::string m_combinedMessage;
};