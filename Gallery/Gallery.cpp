#include <iostream>
#include <string>
#include <iomanip>	// Will help us format the time
#include <ctime>	// Will help us get the current time
#include "MemoryAccess.h"
#include "AlbumManager.h"

#define PROGRAMMER "Michael Stregalev"

int getCommandNumberFromUser()
{
	std::string message("\nPlease enter any command(use number): ");
	std::string numericStr("0123456789");
	
	std::cout << message << std::endl;
	std::string input;
	std::getline(std::cin, input);
	
	while (std::cin.fail() || std::cin.eof() || input.find_first_not_of(numericStr) != std::string::npos) {

		std::cout << "Please enter a number only!" << std::endl;

		if (input.find_first_not_of(numericStr) == std::string::npos) {
			std::cin.clear();
		}

		std::cout << std::endl << message << std::endl;
		std::getline(std::cin, input);
	}
	
	return std::atoi(input.c_str());
}

/*
printDetails()
This function is responsible for printing the program details in the opening screen,
which includes the programmer's name, and the current time and date!

V1.0.1 Mission 6
*/
void printDetails()
{
	// Get the current time
	std::time_t currentTime = std::time(nullptr);		// presented as UTC - need to convert to local time
	std::tm localTime = *std::localtime(&currentTime);	// Convert to local time (* since std::localtime returns a pointer)

	std::cout << "\nBy " << PROGRAMMER << std::endl;	// Name of programmer (me!)
	// Print in the required format of the current time using std::put_time
	std::cout << std::put_time(&localTime, "%H:%M | %d/%m/%Y") << std::endl;
	// %H - current hour in a 24-hour format (00-23)
	// %M - current minutes (0-59)
	// %d - day in the month (01-31)
	// %m - month in the year (01-12)
	// %Y - full current year
}

int main(void)
{
	// V1.0.1
	// Starting to fix bugs in the commands!
	
	// initialization data access
	MemoryAccess dataAccess;

	// initialize album manager
	AlbumManager albumManager(dataAccess);


	std::string albumName;
	std::cout << "Welcome to Gallery!" << std::endl;
	printDetails();
	std::cout << "===================" << std::endl;
	std::cout << "Type " << HELP << " to a list of all supported commands" << std::endl;
	
	do {
		int commandNumber = getCommandNumberFromUser();
		
		try	{
			albumManager.executeCommand(static_cast<CommandType>(commandNumber));
		} catch (std::exception& e) {	
			std::cout << e.what() << std::endl;
		}
	} 
	while (true);
}