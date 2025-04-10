#include "DatabaseAccess.h"
#include "MyException.h"
#include "DatabaseNotOpenException.h"
#include "FailedSQLQueryException.h"
#include "ItemNotFoundException.h"
#include "Album.h"
#include "Picture.h"
#include <list>
#include <map>
#include <iostream>

// CONSTS OF ALL FIELD NAMES - std::strings in order to fit the operator ==

const std::string USERID = "USER_ID";
const std::string NAME = "NAME";
const std::string CREATION = "CREATION_DATE";
const std::string ID = "ID";
const std::string LOCATION = "LOCATION";
const std::string TAGGED_USERS = "TAGGED_USERS";

// DEFINES FOR PREVENTING MAGIC NUMBERS

#define MIN_FIELD_WIDTH 5       // used in setw when printing

// CONSTRUCTOR
DatabaseAccess::DatabaseAccess() :
	_db(nullptr), _openAlbum(nullptr)
{
}

// DECONSTRUCTOR
DatabaseAccess::~DatabaseAccess()
{
    clear();
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
        close();
        throw MyException("Failed to open database.");
    }

    // Enabling foreign keys! CRITICAL - WITHOUT IT FOREIGN KEYS WON'T BE RECOGNIZED (as they are disabled in default)!!
    // This will enable us to use ON DELETE CASADE - which is crucial!!
    // If it doesn't work - a crucial part of the opening of the database hasn't been successful!
    if (!executeSQL("PRAGMA foreign_keys = ON;"))
    {
        close();    // close - as the database is already opened.
        throw MyException("Failed to enable foreign keys.");
    }

    // Initializing the tables for the database!
    // If it does not initalize successfully - return false.
    if (!initializeDatabase())
    {
        close();    // close - as the database is already opened.
        throw MyException("Failed to initialize database.");
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

        // and if there is an album that is open - close it!
        clear();
	}
}

// CLEAR ALL OBJECTS THAT WE DYNAMICALLY ALLOCATED
// as of right now, it is empty - as we did not dynamically allocate any object.
void DatabaseAccess::clear()
{
    // Check if there is an open album.. if there is - delete it, and nullify it.
    if (_openAlbum)
    {
        delete _openAlbum;
        _openAlbum = nullptr;
    }
}

// DELETE AN ALBUM WHEN GIVEN ITS NAME AND OWNER'S ID
void DatabaseAccess::deleteAlbum(const std::string& albumName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will need to also check that the album even exists in the first place, and that the userId leads to an existing user!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesAlbumExists(albumName, userId))
    {
        throw ItemNotFoundException("Album: " + albumName, userId);
    }
    else if (!doesUserExists(userId))
    {
        throw MyException("User does not exist with the id " + std::to_string(userId));
    }

    // Parsing the SQL query..
    std::string sqlQuery = "DELETE FROM ALBUMS WHERE ID = (SELECT ID FROM ALBUMS WHERE USER_ID = " +
                            std::to_string(userId) + " AND NAME = \"" + albumName + "\" LIMIT 1);";

    if (!executeSQL(sqlQuery))
    {
        throw FailedSQLQueryException("Error occurred while trying to delete an album.", sqlQuery);
    }

    // NOW - We need to make sure that the album we just deleted isn't the album we opened, if it is - we will close it
    // BUT - first we will check that there is an album that is open in the first place!
    if (_openAlbum && _openAlbum->getName() == albumName && _openAlbum->getOwnerId() == userId)
    {
        clear();
    }
}

// ADD A TAG OF A USER INTO A PICTURE
void DatabaseAccess::tagUserInPicture(const std::string& albumName, const std::string& pictureName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    // And also check that the userId leads to an existing user.
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(userId))
    {
        throw MyException("User does not exist with the id " + std::to_string(userId));
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
        throw FailedSQLQueryException("Error occurred while trying to tag a user in a picture.", sqlQuery);
    }
}

// UNTAG A USER FROM A PICTURE
void DatabaseAccess::untagUserInPicture(const std::string& albumName, const std::string& pictureName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    // And also that the userId leads to an existing user.
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(userId))
    {
        throw MyException("User does not exist with the id " + std::to_string(userId));
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
        throw FailedSQLQueryException("Error occurred while trying to untag user from picture.", sqlQuery);
    }
}

// CREATE A USER BASED ON A USER OBJECT
void DatabaseAccess::createUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    // And also we will need to check that no user already exists with the ID we want to use
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (doesUserExists(user.getId()))
    {
        throw MyException("User with the id " + std::to_string(user.getId()) + " already exists!");
    }

    // Parsing the SQL query
    std::string sqlQuery = "INSERT INTO USERS (NAME) VALUES (\"" + user.getName() + "\");";

    // Execute the query
    if (!executeSQL(sqlQuery))
    {
        throw FailedSQLQueryException("Error occurred while trying to create a user.", sqlQuery);
    }
}

// DELETE A USER BASED ON A USER OBJECT
void DatabaseAccess::deleteUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(user.getId()))
    {
        throw MyException("User does not exist with the id " + std::to_string(user.getId()));
    }

    // Parsing the SQL query
    std::string sqlQuery = "DELETE FROM USERS WHERE ID = " +
                            std::to_string(user.getId()) + ";";

    // This will also delete all memory of the user - including albums, pictures, and tags that include the user's id!
    // Since i added onto the scheme the ON DELETE CASADE constraint, which when deleting a user will trigger casading deletions.

    // Execute the query
    if (!executeSQL(sqlQuery))
    {
        throw FailedSQLQueryException("Error occurred while trying to delete a user.", sqlQuery);
    }

    // After we deleted a user - we will check if it also deleted the album we currently have that is open!
    // First we will check if there is an album that is open in the first place..
    // if the album was deleted - close it.

    if (_openAlbum && !doesAlbumExists(_openAlbum->getName(), _openAlbum->getOwnerId()))
    {
        clear();
    }
}

// GET ALL ALBUMS IN THE DATABASE
const std::list<Album> DatabaseAccess::getAlbums()
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db) 
    {
        throw DatabaseNotOpenException();
    }

    // Building the list of albums we will get
    std::list<Album> albumList;

    AlbumData albumData{ albumList, this };

    std::string sqlQuery = "SELECT * FROM ALBUMS;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &albumsCallBack, &albumData, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to get the albums.", sqlQuery);
    }

    return albumList;
}

// GET ALL ALBUMS OF A CERTAIN USER
const std::list<Album> DatabaseAccess::getAlbumsOfUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the user exists in the database
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(user.getId()))
    {
        throw MyException("User does not exist with the id " + std::to_string(user.getId()));
    }

    // Building the list of albums we will get
    std::list<Album> albumList;

    AlbumData albumData{ albumList, this };

    std::string sqlQuery = "SELECT * FROM ALBUMS WHERE USER_ID = " + std::to_string(user.getId()) + ';';

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &albumsCallBack, &albumData, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to get the albums of the user " + user.getName() + ".", sqlQuery);
    }

    return albumList;
}

// CREATE AN ALBUM
void DatabaseAccess::createAlbum(const Album& album)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also make sure that the album does not exist in the database!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (doesAlbumExists(album.getName(), album.getOwnerId()))
    {
        throw MyException("The album " + album.getName() + " with the owner id of " + std::to_string(album.getOwnerId()) + " already exists.");
    }

    std::string createAlbumQuery = "INSERT INTO ALBUMS (NAME, CREATION_DATE, USER_ID) VALUES(\"" 
                                   + album.getName() + "\", \"" + album.getCreationDate() + "\", " + std::to_string(album.getOwnerId()) + ");";

    if (!executeSQL(createAlbumQuery))
    {
        throw FailedSQLQueryException("Error occurred while trying to create an album.", createAlbumQuery);
    }
}

// CHECK IF AN ALBUM EXISTS BY NAME AND OWNER ID
bool DatabaseAccess::doesAlbumExists(const std::string& albumName, int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the userId leads to an existing user
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(userId))
    {
        throw MyException("User does not exist with the id " + std::to_string(userId));
    }

    // Start by using the callback function and finding if there is an album that contains the following:
    // the same name as we want, and the same owner id.

    std::list<Album> albums;
    AlbumData albumData{ albums, this };

    std::string sqlQuery = "SELECT * FROM ALBUMS WHERE NAME = \"" + albumName + "\" AND USER_ID = " + std::to_string(userId) + " LIMIT 1;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &albumsCallBack, &albumData, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to find if an album exists.", sqlQuery);
    }

    // If albums contains an album - it does exist! If its empty - it does not exist.
    return !albums.empty();
}

// OPEN AN ALBUM
Album DatabaseAccess::openAlbum(const std::string& albumName)
{
    // Check if the database is open.. as we can only access it when it is open!
    // Check that no album is already open
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (_openAlbum != nullptr)
    {
        throw MyException("Another album is already open. Close it first.");
    }

    // We will find if the album exists, if it doesn't we will throw an exception
    // If it does exist - we will open it.
    std::list<Album> albums;
    AlbumData albumData{ albums, this };

    std::string openAlbumQuery = "SELECT * FROM ALBUMS WHERE NAME = \"" + albumName + "\" LIMIT 1 ;";

    int res = sqlite3_exec(_db, openAlbumQuery.c_str(), &albumsCallBack, &albumData, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to find if an album exists.", openAlbumQuery);
    }

    // If it did not find any album with the name - it does not exist
    if (albums.empty()) 
    {
        throw MyException("Did not find any album with the name of " + albumName);
    }

    // Store the opened album
    _openAlbum = new Album(albums.front());

    // Return the open album
    return *_openAlbum;
}

// CLOSE THE ALBUM
void DatabaseAccess::closeAlbum(Album& pAlbum)
{
    if (!_openAlbum)
    {
        throw MyException("There is no album open currently.");
    }
    else if (!(*_openAlbum == pAlbum))
    {
        throw MyException("Attempting to close the wrong album.");
    }
    clear();
}

// PRINT ALL ALBUMS
void DatabaseAccess::printAlbums()
{
    try
    {
        // Get all the albums in the db
        const std::list<Album> albums = getAlbums();

        // Check if there are albums
        if (albums.empty())
        {
            throw MyException("There are no existing albums.");
        }

        std::cout << "Album list:" << std::endl;
        std::cout << "-----------" << std::endl;

        // Print all info about albums
        for (const Album& album : albums) 
        {
            std::cout << std::setw(MIN_FIELD_WIDTH) << "* " << album;
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}

void DatabaseAccess::addPictureToAlbumByName(const std::string& albumName, const Picture& picture)
{
    // Check if the database is open.. as we can only access it when it is open!
    // Also need to check that an album is open in the first place,
    // and that the album we want to add a picture to is the same album that is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!_openAlbum)
    {
        throw MyException("No album is open. Open one first.");
    }
    else if (!isAlbumOpen(albumName))
    {
        throw MyException("Can't add a picture to an album that you did not open!");
    }
    else if (doesPictureExist(picture))
    {
        throw MyException("The given picture already belongs to an album!");
    }

    std::string insertPictureQuery = "INSERT INTO PICTURES (NAME, LOCATION, CREATION_DATE, ALBUM_ID) "
                                     "VALUES (" + std::to_string(picture.getId()) + ", \"" + picture.getName() + "\", \"" 
                                     + picture.getPath() + "\", \"" + picture.getCreationDate() + "\", (SELECT ID FROM ALBUMS WHERE NAME = \"" +
                                     albumName + "\" LIMIT 1));";

    if (!executeSQL(insertPictureQuery))
    {
        throw FailedSQLQueryException("Error occurred while trying to add the picture to the album.", insertPictureQuery);
    }
}

void DatabaseAccess::removePictureFromAlbumByName(const std::string& albumName, const std::string& pictureName)
{
    // Check if the database is open.. as we can only access it when it is open!
    // Also need to check that an album is open in the first place,
    // and that the album we want to remove a picture from is the same album that is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!_openAlbum)
    {
        throw MyException("No album is open. Open one first.");
    }
    else if (!isAlbumOpen(albumName))
    {
        throw MyException("Can't remove a picture from an album that you did not open!");
    }

    // Deleting the picture from the database...

    std::string deletePictureQuery = "DELETE FROM PICTURES WHERE NAME = \"" + pictureName + "\" "
                                     "AND ALBUM_ID = (SELECT ID FROM ALBUMS WHERE NAME = \"" + albumName + "\" LIMIT 1);";

    if (!executeSQL(deletePictureQuery))
    {
        throw FailedSQLQueryException("Error occurred while trying to remove a picture from the album.", deletePictureQuery);
    }
}

void DatabaseAccess::printUsers()
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }

    // Building the list of users we will get
    std::list<User> usersList;

    std::string sqlQuery = "SELECT * FROM USERS;";

    // We now got the users onto a list!
    int res = sqlite3_exec(_db, sqlQuery.c_str(), &usersCallBack, &usersList, nullptr);

    // If an error occurred..
    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to get the users.", sqlQuery);
    }

    // Printing the users - in case that there are users..

    if (!usersList.empty())
    {
        std::cout << "Users list:" << std::endl;
        std::cout << "-----------" << std::endl;

        for (const auto& user : usersList)
        {
            std::cout << user << std::endl;
        }
    }
    else
    {
        throw MyException("No users found in the database.");
    }
}

User DatabaseAccess::getUser(int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the user exists in the first place!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(userId))
    {
        throw MyException("User does not exist with the id " + std::to_string(userId));
    }

    // USER fits the data the callback function expects
    // DEFAULT VALUES OF USER - WILL BE UPDATED AS WE ALREADY CHECKED THAT THE USER WITH THE ID DOES EXIST.
    User theUser = User(-1, "");

    std::string fetchUserQuery = "SELECT * FROM USERS WHERE ID = " + std::to_string(userId) + " LIMIT 1;";

    // We now got the users onto a list!
    int res = sqlite3_exec(_db, fetchUserQuery.c_str(), &getUserCallBack, &theUser, nullptr);

    // If an error occurred.. (if SQLITE_OK wasn't the return value of the function, or that the ID is invalid..)
    if (res != SQLITE_OK || theUser.getId() == -1)
    {
        throw FailedSQLQueryException("Error occurred while trying to get the users.", fetchUserQuery);
    }

    // Return the user that was input onto the list of users
    return theUser;
}

bool DatabaseAccess::doesUserExists(int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }

    // USER fits the data the callback function expects
    // INCASE that the users data does not change - remains invalid -> the user does not exist
    User theUser = User(-1, "");

    std::string sqlQuery = "SELECT * FROM USERS WHERE ID = " + std::to_string(userId) + " LIMIT 1;";

    // We now got the users onto a list!
    int res = sqlite3_exec(_db, sqlQuery.c_str(), &getUserCallBack, &theUser, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying figure out if a user exists.", sqlQuery);
    }

    // If the ID remains INVALID - no user was found in order to replace its data.
    return !(theUser.getId() == -1);
}

int DatabaseAccess::countAlbumsOwnedOfUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the user even exists in the database..
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(user.getId()))
    {
        throw MyException("User does not exist with the id " + std::to_string(user.getId()));
    }

    // The list of albums that will contain all the albums of the user
    std::list<Album> albumList;

    AlbumData albumData{ albumList, this };

    std::string sqlQuery = "SELECT * FROM ALBUMS WHERE USER_ID = " + std::to_string(user.getId()) + ";";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &albumsCallBack, &albumData, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while find out the amount of albums a user has.", sqlQuery);
    }

    return albumList.size();
}

int DatabaseAccess::countAlbumsTaggedOfUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the user even exists in the database..
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(user.getId()))
    {
        throw MyException("User does not exist with the id " + std::to_string(user.getId()));
    }

    // The list of albums that will contain all the albums of the user
    std::list<Album> albumList;

    AlbumData albumData{ albumList, this };

    std::string sqlQuery = "SELECT DISTINCT ALBUMS.ID FROM ALBUMS INNER JOIN PICTURES ON ALBUMS.ID = PICTURES.ALBUM_ID "
                           "INNER JOIN TAGS ON PICTURES.ID = TAGS.PICTURE_ID WHERE TAGS.USER_ID = " + std::to_string(user.getId()) + ";";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &albumsCallBack, &albumData, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while finding out the amount of albums a user was tagged in.", sqlQuery);
    }

    return albumList.size();
}

int DatabaseAccess::countTagsOfUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the user even exists in the database..
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(user.getId()))
    {
        throw MyException("User does not exist with the id " + std::to_string(user.getId()));
    }

    int countOfTags = 0;

    std::string sqlQuery = "SELECT * FROM TAGS WHERE USER_ID = " + std::to_string(user.getId()) + ";";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &tagsCountCallBack, &countOfTags, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while finding out the amount of times a user has been tagged.", sqlQuery);
    }

    return countOfTags;
}

float DatabaseAccess::averageTagsPerAlbumOfUser(const User& user)
{
    // Get the count of albums tagged by the user
    int albumsTaggedCount = countAlbumsTaggedOfUser(user);

    // In case the user has been tagged 0 times - return 0.
    if (albumsTaggedCount == 0)
    {
        return 0.0f;
    }

    // Get the total count of tags for the user
    int totalTags = countTagsOfUser(user);

    // Calculate and return the average of tags in albums the user has been tagged in.
    return static_cast<float>(totalTags) / albumsTaggedCount;
}

User DatabaseAccess::getTopTaggedUser()
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }

    // USER fits the data the callback function expects
    User theUser = User(-1, "");

    std::string sqlQuery = "SELECT USERS.ID, USERS.NAME, COUNT(TAGS.USER_ID) AS TAG_COUNT "
                           "FROM USERS JOIN TAGS ON USERS.ID = TAGS.USER_ID GROUP BY USERS.ID "
                           "HAVING TAG_COUNT > 0 "
                           "ORDER BY TAG_COUNT DESC LIMIT 1;";

    // We now got the users onto a list!
    int res = sqlite3_exec(_db, sqlQuery.c_str(), &getUserCallBack, &theUser, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to find the most tagged user.", sqlQuery);
    }
    
    // If the ID remains INVALID - no user was found in order to replace its data.
    return theUser;
}

Picture DatabaseAccess::getTopTaggedPicture()
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }

    // PICTURE fits the data the callback expects, it will fill its data and change its information to be valid.
    // DEFAULT VALUES ARE INVALID - IF IT DOES NOT CHANGE, THERE ARE NO PICTURES IN THE DATABASE.
    Picture mostTaggedPicture = Picture(-1, "");

    // GROUP_CONCAT GROUPS ALL OF THE TAGGED USERS OF THE PICTURE ONTO A STRING THAT IS FORMATTED BY A SEPERATING COMMA.
    // IT WILL LOOK LIKE THIS:
    //              TAGGED_USERS
    //              1,3,10,14
    // Which resembles the USER IDS of the users that are tagged in the picture.

    std::string sqlQuery = "SELECT PICTURES.ID, PICTURES.NAME, PICTURES.LOCATION, PICTURES.CREATION_DATE, "
                           "GROUP_CONCAT(TAGS.USER_ID) AS TAGGED_USERS "
                           "FROM PICTURES LEFT JOIN TAGS ON PICTURES.ID = TAGS.PICTURE_ID "
                           "GROUP BY PICTURES.ID ORDER BY COUNT(TAGS.USER_ID) DESC LIMIT 1;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &getPictureCallBack, &mostTaggedPicture, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to find the most tagged picture.", sqlQuery);
    }

    // Returning the most tagged picture (which is now updated by the callback)
    return mostTaggedPicture;
}

std::list<Picture> DatabaseAccess::getTaggedPicturesOfUser(const User& user)
{
    // Check if the database is open.. as we can only access it when it is open!
    // We will also need to check that the user even exists in the database..
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }
    else if (!doesUserExists(user.getId()))
    {
        throw MyException("User does not exist with the id " + std::to_string(user.getId()));
    }

    // The list of pictures the user has been tagged in
    std::list<Picture> taggedPictures;

    // SQL query to select pictures where the user is tagged
    std::string sqlQuery = "SELECT PICTURES.ID, PICTURES.NAME, PICTURES.LOCATION, PICTURES.CREATION_DATE, GROUP_CONCAT(TAGS.USER_ID) AS TAGGED_USERS "
                           "FROM PICTURES JOIN TAGS ON PICTURES.ID = TAGS.PICTURE_ID WHERE TAGS.USER_ID = " + std::to_string(user.getId()) +
                           " GROUP BY PICTURES.ID;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &picturesAvailableCallBack, &taggedPictures, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to find the pictures a user has been tagged in.", sqlQuery);
    }
    else if (taggedPictures.empty())
    {
        throw MyException("There are no pictures in which the user has been tagged in.");
    }

    return taggedPictures;
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

    // Now, update the sequence for USERS and PICTURES
    // We will need to add 'Dummies' in order to let the sequence set actually work.
    if (!executeSQL(R"(
        PRAGMA foreign_keys = OFF;
        INSERT INTO USERS (NAME) VALUES ('Dummy');
        INSERT INTO PICTURES (NAME,LOCATION,CREATION_DATE,ALBUM_ID) VALUES ('A','A','1/1/111',-1);
        UPDATE SQLITE_SEQUENCE SET SEQ = 200 WHERE NAME = 'USERS';
        UPDATE SQLITE_SEQUENCE SET SEQ = 100 WHERE NAME = 'PICTURES';
        DELETE FROM USERS WHERE NAME = 'Dummy';
        DELETE FROM PICTURES WHERE ALBUM_ID = -1;
        PRAGMA foreign_keys = ON;
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

bool DatabaseAccess::isAlbumOpen(const std::string& albumName)
{
    return _openAlbum && albumName == _openAlbum->getName();
}

bool DatabaseAccess::doesPictureExist(const Picture& picture)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }

    // Start by using the callback function and finding if there is an album that contains the following:
    // the same name as we want, and the same owner id.

    std::list<Picture> pictures;

    std::string sqlQuery = "SELECT * FROM PICTURES WHERE ID = " + std::to_string(picture.getId()) + " LIMIT 1;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &albumsCallBack, &pictures, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to find if a picture exists.", sqlQuery);
    }

    // If albums contains an album - it does exist! If its empty - it does not exist.
    return !pictures.empty();
}

int DatabaseAccess::albumsCallBack(void* data, int argc, char** argv, char** colNames)
{
    // Cast the void* data back to our AlbumData
    auto* albumData = static_cast<AlbumData*>(data);

    // Temporary variables to hold album data
    int userId = 0;
    std::string name = "";
    std::string creationDate = "";
    // Temporary variable of the ID of the current album
    int albumId = 0;

    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == USERID)
        {
            userId = std::stoi(argv[i]);
        }
        else if (colNames[i] == NAME)
        {
            name = argv[i];
        }
        else if (colNames[i] == CREATION)
        {
            creationDate = argv[i];
        }
        else if (colNames[i] == ID)
        {
            albumId = std::stoi(argv[i]);
        }
    }

    albumData->albums.emplace_back(userId, name, creationDate);                // Put the new album in the back of the album list
    Album& currentAlbum = albumData->albums.back();                            // Get the last album in the list - the one we just created

    // Now - we can add to each album its pictures (including each tag in each picture)

    std::string pictureQuery = "SELECT ID, NAME, CREATION_DATE, LOCATION FROM PICTURES WHERE ALBUM_ID = " + std::to_string(albumId) + ';';

    PictureData pictureData = { currentAlbum, albumData->db };

    int res = sqlite3_exec(albumData->db->_db, pictureQuery.c_str(), &picturesCallback, &pictureData, nullptr);

    if (res != SQLITE_OK)
    {
        return SQLITE_ERROR;
    }

    return SQLITE_OK;   // We can continue
}

int DatabaseAccess::picturesCallback(void* data, int argc, char** argv, char** colNames)
{
    // Cast the void* back to our PictureData
    auto* pictureData = static_cast<PictureData*>(data);

    // Temporary variables to hold the picture's data
    int pictureId = 0;
    std::string name = "";
    std::string creationDate = "";
    std::string location = "";

    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == ID)
        {
            pictureId = std::stoi(argv[i]);
        }
        else if (colNames[i] == NAME)
        {
            name = argv[i];
        }
        else if (colNames[i] == CREATION)
        {
            creationDate = argv[i];
        }
        else if (colNames[i] == LOCATION)
        {
            location = argv[i];
        }
    }

    pictureData->album.addPicture(Picture(pictureId, name, location, creationDate));        // Add each picture to the album that contains it!
    Picture& currentPicture = pictureData->album.getPictures().back();                             // Getting the picture we just created

    // Now - we can add to each picture its tags
    std::string tagQuery = "SELECT USER_ID FROM TAGS WHERE PICTURE_ID = " + std::to_string(pictureId) + ';';

    TagData tagData = { currentPicture, pictureData->db};

    int res = sqlite3_exec(pictureData->db->_db, tagQuery.c_str(), &tagsCallBack, &tagData, nullptr);

    if (res != SQLITE_OK)
    {
        return SQLITE_ERROR;
    }

    return SQLITE_OK;   // We can continue
}

int DatabaseAccess::tagsCallBack(void* data, int argc, char** argv, char** colNames)
{
    // Casting the void* back to our TagData
    auto* tagData = static_cast<TagData*>(data);

    // Temporary variables to hold the tags's data
    int userId = 0;

    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == USERID)
        {
            userId = std::stoi(argv[i]);
        }
    }

    // Add tag to picture
    tagData->picture.tagUser(userId);

    return SQLITE_OK;   // We can continue
}

int DatabaseAccess::picturesAvailableCallBack(void* data, int argc, char** argv, char** colNames)
{
    // Casting the void* back to our list of pictures
    auto* pictureList = static_cast<std::list<Picture>*>(data);

    // Temporary variables to hold the picture's data
    int pictureId = 0;
    std::string name = "";
    std::string creationDate = "";
    std::string location = "";
    std::string taggedUsers = "";

    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == ID)
        {
            pictureId = std::stoi(argv[i]);
        }
        else if (colNames[i] == NAME)
        {
            name = argv[i];
        }
        else if (colNames[i] == CREATION)
        {
            creationDate = argv[i];
        }
        else if (colNames[i] == LOCATION)
        {
            location = argv[i];
        }
        else if (colNames[i] == TAGGED_USERS)
        {
            taggedUsers = argv[i];
        }
    }

    // Add the picture we found onto the list of pictures
    pictureList->emplace_back(pictureId, name, location, creationDate);

    // Now we need to parse the tagged_users string onto different users ids

    // Parse comma-separated list of tagged user IDs
    std::stringstream ss(taggedUsers);  // Stringstream of the tagged users
    std::string userIdStr;              // String that will contain each time the different user ids

    // Going through each id that is seperated by a comma
    while (std::getline(ss, userIdStr, ','))
    {
        // If there is still a user left, tag it onto the picture!
        if (!userIdStr.empty())
        {
            pictureList->back().tagUser(std::stoi(userIdStr));
        }
    }

    return SQLITE_OK;
}

int DatabaseAccess::usersCallBack(void* data, int argc, char** argv, char** colNames)
{
    // Casting the void* back to our list of users
    auto* usersList = static_cast<std::list<User>*>(data);

    // Temporary variables that will hold the user's data
    int userId = 0;
    std::string userName = "";


    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == ID)
        {
            userId = std::stoi(argv[i]);
        }
        else if (colNames[i] == NAME)
        {
            userName = argv[i];
        }
    }

    // Add the user we found onto the list of users
    usersList->emplace_back(userId, userName);

    return SQLITE_OK;
}

int DatabaseAccess::tagsCountCallBack(void* data, int argc, char** argv, char** colNames)
{
    // Cast the count so far
    auto countTags = static_cast<int*>(data);

    // Increment the count of tags
    (*countTags)++;

    return SQLITE_OK;
}

int DatabaseAccess::getUserCallBack(void* data, int argc, char** argv, char** colNames)
{
    auto user = static_cast<User*>(data);

    // Temporary variables that will hold the user's data
    int userId = 0;
    std::string userName;


    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == ID)
        {
            userId = std::stoi(argv[i]);
        }
        else if (colNames[i] == NAME)
        {
            userName = argv[i];
        }
    }

    user->setId(userId);
    user->setName(userName);

    return SQLITE_OK;
}

int DatabaseAccess::getPictureCallBack(void* data, int argc, char** argv, char** colNames)
{
    auto picture = static_cast<Picture*>(data);

    // Temporary variables to hold the picture's data
    int pictureId = 0;
    std::string name = "";
    std::string creationDate = "";
    std::string location = "";
    std::string taggedUsers = "";

    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == ID)
        {
            pictureId = std::stoi(argv[i]);
        }
        else if (colNames[i] == NAME)
        {
            name = argv[i];
        }
        else if (colNames[i] == CREATION)
        {
            creationDate = argv[i];
        }
        else if (colNames[i] == LOCATION)
        {
            location = argv[i];
        }
        else if (colNames[i] == TAGGED_USERS)
        {
            taggedUsers = argv[i];
        }
    }

    // Update the data into the picture object

    picture->setId(pictureId);
    picture->setName(name);
    picture->setCreationDate(creationDate);
    picture->setPath(location);

    // Now we need to parse the tagged_users string onto different users ids

    // Parse comma-separated list of tagged user IDs
    std::stringstream ss(taggedUsers);  // Stringstream of the tagged users
    std::string userIdStr;              // String that will contain each time the different user ids

    // Going through each id that is seperated by a comma
    while (std::getline(ss, userIdStr, ','))
    {
        // If there is still a user left, tag it onto the picture!
        if (!userIdStr.empty())
        {
            picture->tagUser(std::stoi(userIdStr));
        }
    }

    return SQLITE_OK;
}

int DatabaseAccess::lastUserIdInDatabase()
{
    int latestID = 0;

    std::string sqlQuery = "SELECT MAX(ID) FROM USERS;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &getLastIDCallBack, &latestID, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while getting the last ID of all users.", sqlQuery);
    }

    return latestID;
}

int DatabaseAccess::lastPictureIdInDatabase()
{
    int latestID = 0;

    std::string sqlQuery = "SELECT MAX(ID) FROM PICTURES;";

    int res = sqlite3_exec(_db, sqlQuery.c_str(), &getLastIDCallBack, &latestID, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while getting the last ID of all pictures.", sqlQuery);
    }

    return latestID;
}

int DatabaseAccess::getLastIDCallBack(void* data, int argc, char** argv, char** colNames)
{
    auto lastId = static_cast<int*>(data);

    // Get the ID from the arguments
    // Process each column in the result row
    for (int i = 0; i < argc; i++)
    {
        if (colNames[i] == "MAX(ID)")
        {
            *lastId = std::stoi(argv[i]);
        }
    }

    return SQLITE_OK;
}
