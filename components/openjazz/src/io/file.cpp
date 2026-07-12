
/**
 *
 * @file file.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd August 2005: Created main.c
 * - 22nd July 2008: Created util.c from parts of main.c
 * - 3rd February 2009: Renamed util.c to util.cpp
 * - 3rd February 2009: Created file.cpp from parts of util.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 * Copyright (c) 2015-2026 Carsten Teibes
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Deals with files.
 *
 */


#include "file.h"

#include "io/gfx/video.h"
#include "util.h"
#include "io/log.h"

#include "SDL_wrapper.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <miniz.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
static int diagFileOpenCount = 0;
#endif

#if !(defined(_WIN32) || defined(WII) || defined(PSP))
    #define UPPERCASE_FILENAMES
    #define LOWERCASE_FILENAMES
#endif


/**
 * Try opening a file from the available paths.
 *
 * @param name File name
 * @param pathType Kind of directory
 * @param write Whether or not the file can be written to
 */
File::File (const char* name, int pathType, bool write) :
	file(nullptr), filePath(nullptr), forWriting(write),
	buffer(nullptr), bufferSize(0), bufferPos(0) {

#ifdef ESP_PLATFORM
	diagFileOpenCount++;
	LOG_ERROR("DIAG: file open #%d '%s' heap_internal_free=%u heap_default_free=%u",
		diagFileOpenCount, name,
		(unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
		(unsigned) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
#endif

	Path* path = gamePaths.paths;

	while (path) {

		// skip other paths
		if (pathType != PATH_TYPE_ANY && (path->pathType & pathType) != pathType) {
			path = path->next;
			continue;
		}

		// only allow certain write paths
		if (!write || (pathType & (PATH_TYPE_CONFIG|PATH_TYPE_TEMP)) > 0) {
			if (open(path->path, name, write)) return;
		} else LOG_FATAL("Not allowed to write to %s", name);

		path = path->next;

	}

	LOG_WARN("Could not open file: %s", name);

	throw E_FILE;

}


/**
 * Delete the file object.
 */
File::~File () {

	if (forWriting) {
		fclose(file);
	} else {
#ifdef ESP_PLATFORM
		heap_caps_free(buffer);
#else
		delete[] buffer;
#endif
	}

	LOG_TRACE("Closed file: %s", filePath);

	delete[] filePath;

}


/**
 * Try opening a file from the given path
 *
 * @param path Directory path
 * @param name File name
 * @param write Whether or not the file can be written to
 */
bool File::open (const char* path, const char* name, bool write) {
	const char *fileMode = write ? "wb": "rb";

	// Create the file path for the given directory
	filePath = createString(path, name);

	if (write) {
		// Write-mode: the file may not exist yet, so there's nothing cheap
		// to probe for -- just try to open it directly, as before.
		file = fopen(filePath, fileMode);

#ifdef UPPERCASE_FILENAMES
		if (!file) {
			uppercaseString(filePath + strlen(path));
			file = fopen(filePath, fileMode);
		}
#endif

#ifdef LOWERCASE_FILENAMES
		if (!file) {
			lowercaseString(filePath + strlen(path));
			file = fopen(filePath, fileMode);
		}
#endif
	} else {
		// Read-mode: probe with the cheap access() syscall for each case
		// variant first, and only call fopen() once on a path we already
		// know exists. A failed fopen() leaks a large amount of internal
		// RAM on this target, and this game routinely probes many
		// candidate paths/casings for files that don't exist.
		bool found = (access(filePath, F_OK) == 0);

#ifdef UPPERCASE_FILENAMES
		if (!found) {
			uppercaseString(filePath + strlen(path));
			found = (access(filePath, F_OK) == 0);
		}
#endif

#ifdef LOWERCASE_FILENAMES
		if (!found) {
			lowercaseString(filePath + strlen(path));
			found = (access(filePath, F_OK) == 0);
		}
#endif

		if (!found) {
			delete[] filePath;

			return false;
		}

		// Read-mode: slurp the whole file into a heap buffer using raw
		// POSIX fd I/O (not stdio/fopen). This avoids newlib's per-FILE*
		// stdio stream + lazily-allocated lock entirely, since that
		// machinery does not appear to be reliably reclaimed by fclose()
		// on this target and exhausts internal RAM after enough files are
		// opened over the game's lifetime.
		int fd = ::open(filePath, O_RDONLY);
		if (fd < 0) {
			delete[] filePath;

			return false;
		}

		LOG_DEBUG("Opened file: %s", filePath);

		bufferSize = (int) ::lseek(fd, 0, SEEK_END);
		::lseek(fd, 0, SEEK_SET);

#ifdef ESP_PLATFORM
		LOG_ERROR("DIAG: slurping %s size=%d", filePath, bufferSize);
#endif

#ifdef ESP_PLATFORM
		// This data is only ever touched by the CPU (never DMA'd directly),
		// so it's safe -- and, given how many of these stay alive at once,
		// necessary -- to force it into PSRAM explicitly rather than
		// competing with DMA-dependent buffers (audio, display) for the
		// small internal RAM pool.
		buffer = (unsigned char *) heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
		buffer = new unsigned char[bufferSize];
#endif
		int totalRead = 0;
		while (totalRead < bufferSize) {
			int n = ::read(fd, buffer + totalRead, bufferSize - totalRead);
			if (n <= 0) break;
			totalRead += n;
		}
		if (totalRead != bufferSize)
			LOG_ERROR("Could not read whole file %s (%d of %d bytes read)", filePath, totalRead, bufferSize);

		::close(fd);
		bufferPos = 0;

#ifdef ESP_PLATFORM
		LOG_ERROR("DIAG: slurped %s heap_internal_free=%u heap_default_free=%u",
			filePath,
			(unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			(unsigned) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
#endif

		return true;
	}

	if (!file) {
		delete[] filePath;

		return false;
	}

	LOG_DEBUG("Opened file: %s", filePath);

	return true;
}


/**
 * Check if a file exists.
 *
 * @param fileName The file to check
 * @param pathType Kind of directory
 *
 * @return Whether or not the file exists
 */
bool File::exists (const char * fileName, int pathType) {
	/* FIXME: This currently duplicates code from Constructor/Destructor
	 * and open() method. Maybe merge this into a common function.
	 */

	bool exists = false;

	Path* path = gamePaths.paths;
	while (path) {
		// skip other paths
		if (pathType != PATH_TYPE_ANY && (path->pathType & pathType) != pathType) {
			path = path->next;
			continue;
		}

		// Create the file path for the given directory
		char *fullPath = createString(path->path, fileName);

		// Open the file from the path
		int res = access(fullPath, F_OK);

#ifdef UPPERCASE_FILENAMES
		if (res != 0) {
			uppercaseString(fullPath + strlen(path->path));
			res = access(fullPath, F_OK);
		}
#endif

#ifdef LOWERCASE_FILENAMES
		if (res != 0) {
			lowercaseString(fullPath + strlen(path->path));
			res = access(fullPath, F_OK);
		}
#endif

		delete[] fullPath;

		if(res == 0) {
			exists = true;

			break;
		}

		path = path->next;
	}

	return exists;
}



/**
 * Get the size of the file.
 *
 * @return The size of the file
 */
int File::getSize () {

	if (forWriting) {
		int pos, size;

		pos = ftell(file);
		fseek(file, 0, SEEK_END);
		size = ftell(file);
		fseek(file, pos, SEEK_SET);

		return size;
	}

	return bufferSize;

}


/**
 * Get the current read/write location within the file.
 *
 * @return The current location
 */
int File::tell () {

	if (forWriting) return ftell(file);

	return bufferPos;

}


/**
 * Set the read/write location within the file.
 *
 * @param offset The new offset
 * @param reset Whether to offset from the current location or the start of the file
 */
void File::seek (int offset, bool reset) {

	if (forWriting) {
		fseek(file, offset, reset ? SEEK_SET: SEEK_CUR);
		return;
	}

	bufferPos = reset ? offset : bufferPos + offset;

}

int File::seek (int offset, int origin) {

	if (forWriting) return fseek(file, offset, origin);

	switch (origin) {
		case SEEK_SET: bufferPos = offset; break;
		case SEEK_END: bufferPos = bufferSize + offset; break;
		case SEEK_CUR: default: bufferPos += offset; break;
	}

	return 0;

}

int File::read (void *buf, int size, int count) {

	if (forWriting) return fread(buf, size, count, file);

	int wanted = size * count;
	int available = bufferSize - bufferPos;
	if (available < 0) available = 0;
	int toCopy = (wanted < available) ? wanted : available;

	memcpy(buf, buffer + bufferPos, toCopy);
	bufferPos += toCopy;

	return (size > 0) ? (toCopy / size) : 0;

}


/**
 * Get the next byte from the file, like fgetc().
 *
 * @return The value read, or EOF
 */
int File::getByte () {

	if (forWriting) return fgetc(file);

	if (bufferPos >= bufferSize) return EOF;

	return buffer[bufferPos++];

}


/**
 * Load an unsigned char from the file.
 *
 * @return The value read
 */
unsigned char File::loadChar () {

	return getByte();

}


void File::storeChar (const unsigned char val) {

	fputc(val, file);

}


/**
 * Load an unsigned short int from the file.
 *
 * @return The value read
 */
unsigned short int File::loadShort () {

	unsigned short int val;

	val = getByte();
	val += getByte() << 8;

	return val;

}


/**
 * Load an unsigned short int with an upper limit from the file.
 *
 * @return The value read
 */
unsigned short int File::loadShort (unsigned short int max) {

	unsigned short int val;

	val = loadShort();

	if (val > max) {

		LOG_ERROR("Oversized value %d>%d in file %s", val, max, filePath);

		return max;

	}

	return val;

}


void File::storeShort (const unsigned short int val) {

	fputc(val & 255, file);
	fputc(val >> 8, file);

}


/**
 * Load a signed int from the file.
 *
 * @return The value read
 */
signed int File::loadInt () {

	unsigned int val;

	val = getByte();
	val += getByte() << 8;
	val += getByte() << 16;
	val += getByte() << 24;

	return *((signed int *)&val);

}


void File::storeInt (const signed int val) {

	unsigned int uval;

	uval = *((unsigned int *)&val);

	fputc(uval & 255, file);
	fputc((uval >> 8) & 255, file);
	fputc((uval >> 16) & 255, file);
	fputc(uval >> 24, file);

}


/**
 * Load a string with given length from the file.
 *
 * @return The new string
 */
char * File::loadString (int length) {
	char *string = new char[length + 1];
	int res = read(string, 1, length);

	if (res != length)
		LOG_ERROR("Could not read whole string (%d of %d bytes read)", res, length);

	string[length] = '\0';

	return string;
}


void File::storeString (const char *string) {
	int length = strlen(string);

	storeData(string, length);
}


void File::storeData (const void* data, int length) {

	if(!forWriting) {
		LOG_ERROR("File %s not opened for writing!", filePath);
		return;
	}

	fwrite (data, length, 1, file);

}


/**
 * Load a block of uncompressed data from the file.
 *
 * @param length The length of the block
 *
 * @return Buffer containing the block of data
 */
unsigned char * File::loadBlock (int length) {

	unsigned char *block;

	block = new unsigned char[length];

	int res = read(block, 1, length);

	if (res != length)
		LOG_ERROR("Could not read whole block (%d of %d bytes read)", res, length);

	return block;

}


/**
 * Load a block of RLE compressed data from the file.
 *
 * @param length The length of the uncompressed block
 * @param checkSize Whether or not the RLE block is terminated
 *
 * @return Buffer containing the uncompressed data
 */
unsigned char* File::loadRLE (int length, bool checkSize) {
	int size = 0;

	if (checkSize) {
		// Determine the offset that follows the block
		size = loadShort();
	}
	int start = tell();

	unsigned char* buffer = new unsigned char[length];

	int pos = 0;
	while (pos < length) {
		unsigned char code = loadChar();
		unsigned char amount = code & 127;

		if (code & 128) { // repeat
			unsigned char value = loadChar();

			if (pos + amount >= length) break;

			memset(buffer + pos, value, amount);
			pos += amount;
		} else if (amount) { // copy
			if (pos + amount >= length) break;

			read(buffer + pos, 1, amount);
			pos += amount;
		} else { // end marker
			buffer[pos++] = loadChar();
			break;
		}
	}

	if (checkSize) {
		if (tell() != start + size)
			LOG_DEBUG("RLE block has incorrect size: %d vs. %d", tell() - start, size);

		seek(start + size, true);
	} else {
		LOG_MAX("RLE block was %d bytes long", tell() - start);
	}

	return buffer;

}


/**
 * Skip past a block of RLE compressed data in the file.
 */
void File::skipRLE () {

	int next;

	next = getByte();
	next += getByte() << 8;

	seek(next, SEEK_CUR);

}


/**
 * Load a block of LZ compressed data from the file.
 *
 * @param compressedLength The length of the compressed block
 * @param length The length of the uncompressed block
 *
 * @return Buffer containing the uncompressed data
 */
unsigned char* File::loadLZ (int compressedLength, int length) {

	unsigned char* compressedBuffer;
	unsigned char* buffer;

	compressedBuffer = loadBlock(compressedLength);

	buffer = new unsigned char[length];

	uncompress(buffer, (unsigned long int *)&length, compressedBuffer, compressedLength);

	delete[] compressedBuffer;

	return buffer;

}


/**
 * Load a terminated string from the file.
 *
 * @param maxSize maximum length of field in the file
 *
 * @return The new string
 */
char * File::loadTerminatedString (int maxSize) {
	char *string;
	int length = loadChar();

	if (maxSize > 0 && length > maxSize) {
		LOG_WARN("Trimming oversized terminated string (%d > %d)", length, maxSize);
		length = maxSize;
	}

	if (length) {
		string = loadString(length);
	} else {
		string = new char[1];
		string[0] = '\0';
	}

	// Skip until end of field
	if(maxSize > length) {
		seek(maxSize - length);
	}

	return string;
}


/**
 * Load a 8.3 file name from the file.
 *
 * @return The new string
 */
char * File::loadFileName () {
	int length = 0;
	char *string = new char[13];

	for (int i = 0; i < 9; i++) {
		string[i] = loadChar();

		if (string[i] == '.') {
			string[++i] = loadChar();
			string[++i] = loadChar();
			string[++i] = loadChar();
			i++;

			break;
		}

		length = i;
	}

	string[length] = 0;

	return string;
}


/**
 * Load RLE compressed graphical data from the file.
 *
 * @param width The width of the image to load
 * @param height The height of the image to load
 * @param checkSize Whether or not the RLE block is terminated
 *
 * @return SDL surface containing the loaded image
 */
SDL_Surface* File::loadSurface (int width, int height, bool checkSize) {

	SDL_Surface* surface;
	unsigned char* pixels;

	pixels = loadRLE(width * height, checkSize);

	surface = video.createSurface(pixels, width, height);

	delete[] pixels;

	return surface;

}


/**
 * Load a block of scrambled pixel data from the file.
 *
 * @param length The length of the block
 *
 * @return Buffer containing the de-scrambled data
 */
unsigned char* File::loadPixels  (int length) {

	unsigned char* pixels;
	unsigned char* sorted;
	int count;

	sorted = new unsigned char[length];

	pixels = loadBlock(length);

	// Rearrange pixels in correct order
	for (count = 0; count < length; count++) {

		sorted[count] = pixels[(count >> 2) + ((count & 3) * (length >> 2))];

	}

	delete[] pixels;

	return sorted;

}


/**
 * Load a block of scrambled and masked pixel data from the file.
 *
 * @param length The length of the block
 * @param key The transparent pixel value
 *
 * @return Buffer containing the de-scrambled data
 */
unsigned char* File::loadPixels (int length, int key) {

	unsigned char* pixels;
	unsigned char* sorted;
	unsigned char mask = 0;
	int count;


	sorted = new unsigned char[length];
	pixels = new unsigned char[length];


	// Read the mask
	// Each mask pixel is either 0 or 1
	// Four pixels are packed into the lower end of each byte
	for (count = 0; count < length; count++) {

		if (!(count & 3)) mask = getByte();
		pixels[count] = (mask >> (count & 3)) & 1;

	}

	// Pixels are loaded if the corresponding mask pixel is 1, otherwise
	// the transparent index is used. Pixels are scrambled, so the mask
	// has to be scrambled the same way.
	for (count = 0; count < length; count++) {

		sorted[(count >> 2) + ((count & 3) * (length >> 2))] = pixels[count];

	}

	// Read pixels according to the scrambled mask
	for (count = 0; count < length; count++) {

		// Use the transparent pixel
		pixels[count] = key;

		if (sorted[count] == 1) {

			// The unmasked portions are transparent, so no masked
			// portion should be transparent.
			while (pixels[count] == key) pixels[count] = getByte();

		}

	}

	// Rearrange pixels in correct order
	for (count = 0; count < length; count++) {

		sorted[count] = pixels[(count >> 2) + ((count & 3) * (length >> 2))];

	}

	delete[] pixels;

	return sorted;

}


/**
 * Load a palette from the file.
 *
 * @param palette The palette to be filled with loaded colours
 * @param rle Whether or not the palette data is RLE-encoded
 */
void File::loadPalette (SDL_Color* palette, bool rle) {

	unsigned char* buffer;
	int count;

	if (rle) buffer = loadRLE(MAX_PALETTE_COLORS * 3);
	else buffer = loadBlock(MAX_PALETTE_COLORS * 3);

	for (count = 0; count < MAX_PALETTE_COLORS; count++) {

		// Palette entries are 6-bit
		// Shift them upwards to 8-bit, and fill in the lower 2 bits
		palette[count].r = (buffer[count * 3] << 2) + (buffer[count * 3] >> 4);
		palette[count].g = (buffer[(count * 3) + 1] << 2) + (buffer[(count * 3) + 1] >> 4);
		palette[count].b = (buffer[(count * 3) + 2] << 2) + (buffer[(count * 3) + 2] >> 4);

	}

	delete[] buffer;

}


PathMgr::PathMgr():
	paths(NULL), has_config(false), has_temp(false) {

};

PathMgr::~PathMgr() {
	delete paths;
};

bool PathMgr::add(char* newPath, int newPathType) {

	// Check for CWD
	if(!strlen(newPath)) {
		delete[] newPath;

		char cwd[1024];
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			newPath = createString(cwd);
		} else {
			LOG_WARN("Could not get current working directory!");
			return false;
		}
	}

	// Append a directory separator if necessary
	if (newPath[strlen(newPath) - 1] != OJ_DIR_SEP) {
		char* tmp = createString(newPath, OJ_DIR_SEP_STR);
		delete[] newPath;
		newPath = tmp;
	}

	// all paths need to be readable
	if (access(newPath, R_OK) != 0) {
		LOG_TRACE("Path '%s' is not readable, ignoring!", newPath);
		delete[] newPath;
		return false;
	}

	// ignore, if already present
	if(has_config) newPathType &= ~PATH_TYPE_CONFIG;
	if(has_temp) newPathType &= ~PATH_TYPE_TEMP;

	// config and temp dir need to be writeable
	if ((newPathType & (PATH_TYPE_CONFIG|PATH_TYPE_TEMP)) > 0) {
		if (access(newPath, W_OK) != 0) {
			LOG_WARN("Path '%s' is not writeable, disabling!", newPath);

			newPathType &= ~PATH_TYPE_CONFIG;
			newPathType &= ~PATH_TYPE_TEMP;
		}
	}

	if(newPathType == PATH_TYPE_INVALID) {
		delete[] newPath;
		return false;
	}

	// we only need one directory for these
	if (newPathType & PATH_TYPE_CONFIG) has_config = true;
	if (newPathType & PATH_TYPE_TEMP) has_temp = true;

	// Finally add
	paths = new Path(paths, newPath, newPathType);

	return true;
}

const char *PathMgr::getTemp() {
	Path* path = gamePaths.paths;

	while (path) {

		// skip other paths
		if ((path->pathType & PATH_TYPE_TEMP) != PATH_TYPE_TEMP) {
			path = path->next;
			continue;
		}

		return path->path;

	}

	LOG_WARN("Could not find temp path");
	return "";
}

/**
 * Create a new directory path object.
 *
 * @param newNext Next path
 * @param newPath The new path
 */
Path::Path (Path* newNext, char* newPath, int newPathType) {

	char pathInfo[10] = {};
	if(newPathType & PATH_TYPE_SYSTEM) strcat(pathInfo, "S");
	if(newPathType & PATH_TYPE_CONFIG) strcat(pathInfo, "C");
	if(newPathType & PATH_TYPE_GAME) strcat(pathInfo, "G");
	if(newPathType & PATH_TYPE_TEMP) strcat(pathInfo, "T");
	if(newPathType & PATH_TYPE_ANY) strcat(pathInfo, "A");

	LOG_DEBUG("Adding '%s' to the path list [%s]", newPath, pathInfo);

	next = newNext;
	path = newPath;
	pathType = newPathType;

}


/**
 * Delete the directory path object.
 */
Path::~Path () {

	if (next) delete next;
	delete[] path;

}
