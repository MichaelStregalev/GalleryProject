#pragma once

#include "IDataAccess.h"
#include "sqlite3.h"
#include "User.h"
#include <string.h>

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

private:

	// FIELDS
	// The DB itself
	sqlite3* _db;

	// PRIVATE METHODS FOR MAKING THE REST EFFICIENT

	bool initializeDatabase();				// initialize the database - won't cause errors even if already initialized
											// Returns true if initialization done successfully - otherwise false.

	// HELPER METHOD - EXECUTE SQL QUERIES, RETURN TRUE OR FALSE IF IT DID OR DID NOT WORK SUCCESSFULLY
	bool executeSQL(const std::string& query);
};