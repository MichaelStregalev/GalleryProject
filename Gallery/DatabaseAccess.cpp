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

    // Enabling foreign keys! CRITICAL - WITHOUT IT FOREIGN KEYS WON'T BE RECOGNIZED!!
    // This will enable us to use on delete casade - which is crucial!!
    sqlite3_exec(_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    initializeDatabase();   // Initializing the tables for the database!

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

// CLEAR THE DATABASE FROM ALL DATA, BUT KEEP THE TABLES
void DatabaseAccess::clear()
{
    // Will only be able to clear in case the database is open...
    if (_db != nullptr)
    {
        const char* clearTables[] = 
        {
            "DELETE FROM PICTURES;",
            "DELETE FROM ALBUMS;",
            "DELETE FROM USERS;",
            "DELETE FROM TAGS;"
        };

        char* errMessage = nullptr;

        for (int i = 0; i < TABLE_AMOUNT; ++i) 
        {
            if (sqlite3_exec(_db, clearTables[i], nullptr, nullptr, &errMessage) != SQLITE_OK)
            {
                std::cout << "Clear failed: " << errMessage << std::endl;
                sqlite3_free(errMessage);
                break;
            }
        }
    }
}

bool DatabaseAccess::initializeDatabase()
{
    // Initialization query of all 4 tables

    const char* createTables = R"(
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
        )";

    char* errMessage = nullptr;

    // Execute the query
    int res = sqlite3_exec(_db, createTables, nullptr, nullptr, &errMessage);

    // If the query lead to an error...
    if (res != SQLITE_OK)
    {
        std::cout << "Initilization of tables failed: " << errMessage << std::endl;
        sqlite3_free(errMessage);
        return false;
    }

    return true;
}