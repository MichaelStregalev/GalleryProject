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
    if (_db) 
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
	if (_db)
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

// DELETE AN ALBUM WHEN GIVEN ITS NAME AND OWNER'S ID
void DatabaseAccess::deleteAlbum(const std::string& albumName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        std::cout << "The database is not open!" << std::endl;
        return;
    }

    // Parsing the SQL query..
    std::string sqlQuery = "DELETE FROM ALBUMS WHERE ID = (SELECT ID FROM ALBUMS WHERE USER_ID = " +
                            std::to_string(userId) + " AND NAME = \"" + albumName + "\" LIMIT 1);";

    if (!executeSQL(sqlQuery))
    {
        std::cout << "Error occurred while trying to delete an album." << std::endl;
    }
}

// ADD A TAG OF A USER INTO A PICTURE
void DatabaseAccess::tagUserInPicture(const std::string& albumName, const std::string& pictureName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        std::cout << "The database is not open!" << std::endl;
        return;
    }

    // Parsing the SQL query
    std::string sqlQuery = 
        "INSERT INTO TAGS (PICTURE_ID, USER_ID) VALUES(("
        "SELECT PICTURES.ID FROM PICTURES "
        "INNER JOIN ALBUMS ON PICTURES.ALBUM_ID = ALBUMS.ID "
        "WHERE ALBUMS.NAME = \"" + albumName + "\" "
        "AND PICTURES.NAME = \"" + pictureName + "\" "
        "LIMIT 1), " + std::to_string(userId) + ");";

    if (!executeSQL(sqlQuery))
    {
        std::cout << "Error occurred while trying to tag a user in a picture." << std::endl;
    }
}

// UNTAG A USER FROM A PICTURE
void DatabaseAccess::untagUserInPicture(const std::string& albumName, const std::string& pictureName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        std::cout << "The database is not open!" << std::endl;
        return;
    }

    // Parsing the SQL query
    std::string sqlQuery =
        "DELETE FROM TAGS WHERE USER_ID = " + std::to_string(userId) + " "
        "AND PICTURE_ID = ("
        "SELECT PICTURES.ID FROM PICTURES "
        "INNER JOIN ALBUMS ON PICTURES.ALBUM_ID = ALBUMS.ID "
        "WHERE ALBUMS.NAME = \"" + albumName + "\" "
        "AND PICTURES.NAME = \"" + pictureName + "\" "
        "LIMIT 1"
        ");";

    // Execute the query
    if (!executeSQL(sqlQuery))
    {
        std::cout << "Error occurred while trying to untag user from picture." << std::endl;
    }
}

// CREATE A USER BASED ON A USER OBJECT
void DatabaseAccess::createUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        std::cout << "The database is not open!" << std::endl;
        return;
    }

    // Parsing the SQL query
    std::string sqlQuery = "INSERT INTO USERS (ID, NAME) VALUES (" +
                            std::to_string(user.getId()) + ", \"" +
                            user.getName() + "\");";

    // Execute the query
    if (!executeSQL(sqlQuery))
    {
        std::cout << "Error occurred while trying to create a user." << std::endl;
    }
}

// DELETE A USER BASED ON A USER OBJECT
void DatabaseAccess::deleteUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        std::cout << "The database is not open!" << std::endl;
        return;
    }

    // Parsing the SQL query
    std::string sqlQuery = "DELETE FROM USERS WHERE ID = " +
                            std::to_string(user.getId()) + ";";

    // This will also delete all memory of the user - including albums, pictures, and tags that include the user's id!
    // Since i added onto the scheme the ON DELETE CASADE constraint, which when deleting a user will trigger casading deletions.

    // Execute the query
    if (!executeSQL(sqlQuery))
    {
        std::cout << "Error occurred while trying to delete a user." << std::endl;
    }
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
    if (!_db) 
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