#include "DatabaseAccess.h"
#include "MyException.h"
#include "DatabaseNotOpenException.h"
#include "FailedSQLQueryException.h"
#include "ItemNotFoundException.h"
#include "Album.h"
#include "Picture.h"
#include <iostream>

// DEFINE CONSTS OF ALL FIELD NAMES

#define USERID "USER_ID"
#define NAME "NAME"
#define CREATION "CREATION_DATE"
#define ID "ID"
#define LOCATION "LOCATION"

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
    std::string sqlQuery = "INSERT INTO USERS (ID, NAME) VALUES (" +
                            std::to_string(user.getId()) + ", \"" +
                            user.getName() + "\");";

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

    // Inserting onto the PICTURES TABLE the picture given including the ID -
    // Since we already checked that there is no picture with the given ID.

    std::string insertPictureQuery = "INSERT INTO PICTURES (ID, NAME, LOCATION, CREATION_DATE, ALBUM_ID) "
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

    // Printing the users

    std::cout << "Users list:" << std::endl;
    std::cout << "-----------" << std::endl;

    for (const auto& user : usersList)
    {
        std::cout << user << std::endl;
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

    // The list of users which will be used in order to fetch the user with the id.
    // fits the data the callback function expects
    std::list<User> usersList;

    std::string fetchUserQuery = "SELECT * FROM USERS WHERE ID = " + std::to_string(userId) + " LIMIT 1;";

    // We now got the users onto a list!
    int res = sqlite3_exec(_db, fetchUserQuery.c_str(), &usersCallBack, &usersList, nullptr);

    // If an error occurred..
    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying to get the users.", fetchUserQuery);
    }

    // Return the user that was input onto the list of users
    return usersList.front();
}

bool DatabaseAccess::doesUserExists(int userId)
{
    // Check if the database is open.. as we can only access it when it is open!
    if (!_db)
    {
        throw DatabaseNotOpenException();
    }

    // Building the list of users we will use to figure out if a user with the id given exists.
    std::list<User> usersList;

    std::string sqlQuery = "SELECT * FROM USERS WHERE ID = " + std::to_string(userId) + " LIMIT 1;";

    // We now got the users onto a list!
    int res = sqlite3_exec(_db, sqlQuery.c_str(), &usersCallBack, &usersList, nullptr);

    if (res != SQLITE_OK)
    {
        throw FailedSQLQueryException("Error occurred while trying figure out if a user exists.", sqlQuery);
    }

    return !usersList.empty();
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

    // Add the picture we found onto the list of pictures
    pictureList->emplace_back(pictureId, name, location, creationDate);

    return SQLITE_OK;
}

int DatabaseAccess::usersCallBack(void* data, int argc, char** argv, char** colNames)
{
    // Casting the void* back to our list of users
    auto* usersList = static_cast<std::list<User>*>(data);

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

    // Add the user we found onto the list of users
    usersList->emplace_back(userId, userName);

    return SQLITE_OK;
}