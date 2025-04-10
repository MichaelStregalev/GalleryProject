#pragma once

#include "IDataAccess.h"
#include "sqlite3.h"
#include "User.h"
#include "Album.h"
#include <list>
#include <string.h>
#include <unordered_map>

// Define const presenting the Gallery's DB file!
#define DB_FILE "GalleryDB.sqlite"
// Define const presenting the amount of tables in the DB
#define TABLE_AMOUNT 4

class DatabaseAccess : public IDataAccess
{
public:

	// CONSTRUCTOR & DECONSTRUCTOR
	DatabaseAccess();
	virtual ~DatabaseAccess();

	// BASE METHODS - LEVEL 1 OF V1.0.2

	virtual bool open() override;			// open the database
	virtual void close() override;			// close the database
	virtual void clear() override;			// clear all dynamically allocated objects

	// METHODS - LEVEL 2 OF V1.0.2

	virtual void deleteAlbum(const std::string& albumName, int userId);
	virtual void tagUserInPicture(const std::string& albumName, const std::string& pictureName, int userId);
	virtual void untagUserInPicture(const std::string& albumName, const std::string& pictureName, int userId);
	virtual void createUser(const User& user);
	virtual void deleteUser(const User& user);

	// METHODS - LEVEL 3 OF V1.0.2

	virtual const std::list<Album> getAlbums();
	virtual const std::list<Album> getAlbumsOfUser(const User& user);
	virtual void createAlbum(const Album& album);
	virtual bool doesAlbumExists(const std::string& albumName, int userId);
	virtual Album openAlbum(const std::string& albumName);
	virtual void closeAlbum(Album& pAlbum);
	virtual void printAlbums();

	virtual void addPictureToAlbumByName(const std::string& albumName, const Picture& picture);
	virtual void removePictureFromAlbumByName(const std::string& albumName, const std::string& pictureName);

	virtual void printUsers();
	virtual User getUser(int userId);
	virtual bool doesUserExists(int userId);

	virtual int countAlbumsOwnedOfUser(const User& user);
	virtual int countAlbumsTaggedOfUser(const User& user);
	virtual int countTagsOfUser(const User& user);
	virtual float averageTagsPerAlbumOfUser(const User& user);

	virtual User getTopTaggedUser();
	virtual Picture getTopTaggedPicture();
	virtual std::list<Picture> getTaggedPicturesOfUser(const User& user);


	// The following functions are built in order to help us interact with the AlbumManager!

	// Get the last userID in the database (the latest user's ID)
	int lastUserIdInDatabase();
	// Get the last pictureID in the database (the lastest picture's ID)
	int lastPictureIdInDatabase();


private:

	// FIELDS!!
	
	// The DB itself
	sqlite3* _db;

	// The album that is currently open!!
	Album* _openAlbum;


	// PRIVATE METHODS FOR MAKING THE REST EFFICIENT

	bool initializeDatabase();				// initialize the database - won't cause errors even if already initialized
											// Returns true if initialization done successfully - otherwise false.

	// HELPER METHOD - EXECUTE SQL QUERIES, RETURN TRUE OR FALSE IF IT DID OR DID NOT WORK SUCCESSFULLY
	bool executeSQL(const std::string& query);

	// PRIVATE HELPER METHODS

	bool isAlbumOpen(const std::string& albumName);		// Returns true if the name of the album given is the same as the album that is open!
	bool doesPictureExist(const Picture& picture);		// Returns ture if the picture given exists in the database
	// CALLBACK FUNCTIONS

	// Callback function of getting albums onto a list
	static int albumsCallBack(void* data, int argc, char** argv, char** colNames);
	// Callback function of getting all the pictures inside an album
	static int picturesCallback(void* data, int argc, char** argv, char** colNames);
	// Callback function of tagging a user in a picture given to it
	static int tagsCallBack(void* data, int argc, char** argv, char** colNames);
	// Callback function of getting all the available pictures in the database
	static int picturesAvailableCallBack(void* data, int argc, char** argv, char** colNames);
	// Callback function of getting all the users onto a list
	static int usersCallBack(void* data, int argc, char** argv, char** colNames);
	// Callback function of getting the amount of tags of a user
	static int tagsCountCallBack(void* data, int argc, char** argv, char** colNames);
	// Callback function of getting a specific user
	static int getUserCallBack(void* data, int argc, char** argv, char** colNames);
	// Callback function of getting a single picture
	static int getPictureCallBack(void* data, int argc, char** argv, char** colNames);



	// STRUCTURES THAT WILL HELP US IN THE CALLBACK FUNCTIONS
	// Each structure will hold the information needed, and also the database object!!

	struct AlbumData 
	{
		std::list<Album>& albums;
		DatabaseAccess* db;
	};

	struct PictureData 
	{
		Album& album;
		DatabaseAccess* db;
	};

	struct TagData 
	{
		Picture& picture;
		DatabaseAccess* db;
	};

	// And the callback function for them
	static int getLastIDCallBack(void* data, int argc, char** argv, char** colNames);
};