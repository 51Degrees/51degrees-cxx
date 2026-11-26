/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2026 51 Degrees Mobile Experts Limited, Davidson House,
 * Forbury Square, Reading, Berkshire, United Kingdom RG1 3EU.
 *
 * This Original Work is licensed under the European Union Public Licence
 * (EUPL) v.1.2 and is subject to its terms as set out below.
 *
 * If a copy of the EUPL was not distributed with this file, You can obtain
 * one at https://opensource.org/licenses/EUPL-1.2.
 *
 * The 'Compatible Licences' set out in the Appendix to the EUPL (as may be
 * amended by the European Commission) shall be deemed incompatible for
 * the purposes of the Work and the provisions of the compatibility
 * clause in Article 5 of the EUPL shall not apply.
 *
 * If using the Work as, or as part of, a network application, by
 * including the attribution notice(s) required under Article 5 of the EUPL
 * in the end user terms of the application under an appropriate heading,
 * such notice(s) shall fulfill the requirements of that article.
 * ********************************************************************* */
 
#include "pch.h"
#include "FileHandle.hpp"
#include "Base.hpp"
#include "../../src/common/Exceptions.hpp"

using namespace FiftyoneDegrees::Common;

/**
 * Opens the file specified by the file name, and writes the data provided to
 * it. A file pool is then created with the concurrency specified. If the file
 * already exists, then it is deleted first. If the file cannot be written,
 * opened, or opened by a file pool, the test fails with a message naming the
 * file, and isOpen() is false.
 * @param fileName full path to the file to create, to which the process id
 * is added
 * @param data bytes to write to the file
 * @param number of bytes to write
 * @param concurrency to use for the file pool
 */
FileHandle::FileHandle(
	const char *fileName,
	void *data,
	size_t dataSize,
	uint16_t concurrency) {
	FIFTYONE_DEGREES_EXCEPTION_CREATE
	_fileName = testProcessName(fileName);
	_file = NULL;
	_filePoolReady = false;
	fiftyoneDegreesFileDelete(_fileName.c_str());
	fiftyoneDegreesStatusCode status =
		fiftyoneDegreesFileWrite(_fileName.c_str(), data, dataSize);
	checkStatus(status, "written");
	if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
		return;
	}
	status = fiftyoneDegreesFileOpen(_fileName.c_str(), &_file);
	checkStatus(status, "opened");
	if (status != FIFTYONE_DEGREES_STATUS_SUCCESS) {
		_file = NULL;
		return;
	}
	// A failure the exception records is also returned as the status, so it
	// is reported with the file name rather than thrown. The destructor then
	// still closes the file and deletes it.
	status = fiftyoneDegreesFilePoolInit(
		&_filePool,
		_fileName.c_str(),
		concurrency,
		exception);
	checkStatus(status, "opened by a file pool");
	_filePoolReady = status == FIFTYONE_DEGREES_STATUS_SUCCESS;
}

/**
 * Close the file and file pool, then delete the file.
 */
FileHandle::~FileHandle() {
	if (_filePoolReady) {
		fiftyoneDegreesFilePoolRelease(&_filePool);
	}
	if (_file != NULL) {
		fclose(_file);
	}
	fiftyoneDegreesFileDelete(_fileName.c_str());
}

/**
 * Gets whether the file was written and opened, and its file pool created.
 * @return true if the file and the file pool can be used
 */
bool FileHandle::isOpen() {
	return _file != NULL && _filePoolReady;
}

/**
 * Gets the name of the file.
 * @return file name
 */
const char* FileHandle::getFileName() {
	return _fileName.c_str();
}

/**
 * Gets the open file at the position it was left at.
 * @return file
 */
FILE* FileHandle::getFile() {
	return _file;
}

/**
 * Gets the file pool.
 * @return file pool
 */
fiftyoneDegreesFilePool* FileHandle::getFilePool() {
	return &_filePool;
}

/**
 * Check that the file operation succeeded. Fails the test if it did not.
 * @param status returned by the file operation
 * @param action what the operation did to the file, for the message
 */
void FileHandle::checkStatus(
	fiftyoneDegreesStatusCode status,
	const char *action) {
	ASSERT_EQ(status, FIFTYONE_DEGREES_STATUS_SUCCESS) <<
		"The file " << _fileName << " could not be " << action << ".";
}
