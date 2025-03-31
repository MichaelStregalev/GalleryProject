#include "DatabaseAccess.h"
#include <iostream>

// CONSTRUCTOR
DatabaseAccess::DatabaseAccess() :
	_db(nullptr)
{
}

// DECONSTRUCTOR
DatabaseAccess::~DatabaseAccess()
{
	close();
}

// OPEN THE DATABASE & INITIALIZE
bool DatabaseAccess::open()
{
    // Checking if already open another database
    if (_db != nullptr) 
    {  
        return true;
    }

    // Trying to connect to the sqlite database
    int res = sqlite3_open(DB_FILE, &_db);
    if (res != SQLITE_OK) 
    {
        std::cout << "Failed to open database." << std::endl;
        _db = nullptr;
        return false;
    }

    // Enabling foreign keys! CRITICAL - WITHOUT IT FOREIGN KEYS WON'T BE RECOGNIZED (as they are disabled in default)!!
    // This will enable us to use ON DELETE CASADE - which is crucial!!
    // If it doesn't work - a crucial part of the opening of the database hasn't been successful!
    if (!executeSQL("PRAGMA foreign_keys = ON;"))
    {
        std::cout << "Failed to enable foreign keys." << std::endl;
        close();    // close - as the database is already opened.
        return false;
    }

    // Initializing the tables for the database!
    // If it does not initalize successfully - return false.
    if (!initializeDatabase())
    {
        std::cout << "Failed to initialize database." << std::endl;
        close();    // close - as the database is already opened.
        return false;
    }

    // If we got here - we successfully opened our database
    return true;

}

// CLOSE THE DATABASE
void DatabaseAccess::close()
{
	// Check if _db points to an open database - if it does, close it :)
	if (_db != nullptr)
	{
		sqlite3_close(_db);
		_db = nullptr;
	}
}

// CLEAR ALL OBJECTS THAT WE DYNAMICALLY ALLOCATED
// as of right now, it is empty - as we did not dynamically allocate any object.
void DatabaseAccess::clear()
{
    
}

bool DatabaseAccess::initializeDatabase()
{
    // Initialization of the 4 tables
    // If it doesn't work - return false.

    if (!executeSQL(R"(
            CREATE TABLE IF NOT EXISTS USERS(
                ID INTEGER PRIMARY KEY AUTOINCREMENT,
                NAME TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS ALBUMS(
                ID INTEGER PRIMARY KEY AUTOINCREMENT,
                NAME TEXT NOT NULL,
                CREATION_DATE DATE NOT NULL,
                USER_ID INTEGER NOT NULL,
                FOREIGN KEY (USER_ID) REFERENCES USERS(ID) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS PICTURES(
                ID INTEGER PRIMARY KEY AUTOINCREMENT,
                NAME TEXT NOT NULL,
                LOCATION TEXT NOT NULL,
                CREATION_DATE DATE NOT NULL,
                ALBUM_ID INTEGER NOT NULL,
                FOREIGN KEY (ALBUM_ID) REFERENCES ALBUMS(ID) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS TAGS(
                ID INTEGER PRIMARY KEY AUTOINCREMENT,
                PICTURE_ID INTEGER NOT NULL,
                USER_ID INTEGER NOT NULL,
                FOREIGN KEY (PICTURE_ID) REFERENCES PICTURES(ID) ON DELETE CASCADE,
                FOREIGN KEY (USER_ID) REFERENCES USERS(ID) ON DELETE CASCADE,
                UNIQUE(PICTURE_ID, USER_ID)
            );
        )"))
    {
        return false;
    }
    
    return true;
}

bool DatabaseAccess::executeSQL(const std::string& query)
{
    // Before executing - we will check that the database is open
    if (_db == nullptr) 
    {
        return false;
    }

    char* errMsg = nullptr;
    int result = sqlite3_exec(_db, query.c_str(), nullptr, nullptr, &errMsg);

    // Check if the execution has been done successfully
    if (result != SQLITE_OK)
    {
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}
