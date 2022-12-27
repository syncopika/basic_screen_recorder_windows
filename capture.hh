// capture.hh
// these functions define the screen capture behavior 

#ifndef CAPTURE_H
#define CAPTURE_H

#include <iostream>   // for debugging / errors
#include <fstream>    // for reading in bmp image data
#include <array>      // for reading in bmp image data
#include <windows.h>  // for screen capture 
#include <gdiplus.h>  // for screen capture 
#include <memory>     // for screen capture, unique pointer
#include <string>     // for int to string conversion 
#include <sstream>    // for int to string conversion 
#include <vector>     // used throughout
#include <map>
#include <ctime>
#include "bmp_helper.hh" // filter function declarations
#include "resources.h"   // contains definition for ID_PROCESS_FRAME


// struct to provide arguments needed for snapshots
struct WindowInfo {
	int duration;
	int timeDelay;
	int selectedFilter; // this is the index of the filter in the dropdown box 

	bool ffmpegExists;

	std::string directory; // this was for selecting a directory to read bmps from but can probably be removed now
	std::string tempDirectory; // the directory to place snapshots in
	std::wstring captionText;
	HWND mainWindow; // main window so the worker thread can post messages to its queue 
	std::map<int, std::wstring> filters;
    
	// parameters from the parameters page 
	COLORREF selectionWindowColor;
	float saturationValue;
	int mosaicChunkSize; 		 // for mosaic filter 
	int outlineColorDiffLimit;   // for outline filter 
	int voronoiNeighborConstant; // for Voronoi filter
	int blurFactor;              // for blur filter
	bool getCursor;
	bool cleanupFiles;
};

// convert an int to string 
std::string intToString(int i);

// check if a point is within a particular rect
bool ptIsInRange(POINT& start, int width, int height, POINT& pt);

// screen capturing code 
int getEncoderClsid(const WCHAR* format, CLSID* pClsid);
void bitmapToBMP(HBITMAP hbmpImage, int width, int height, std::string filename);
bool screenCapture(int x, int y, int width, int height, const char* filename, bool getCursor);

// this function relies on all the above 
// the result is creating a temp folder and populating it with screenshots
// and applies filters / caption to each frame
void getSnapshots(
	int nImages, 
	int delay, 
	int x, 
	int y, 
	int width, 
	int height, 
	std::vector<uint8_t> (*filter)(const std::wstring&, WindowInfo*), 
	WindowInfo* captureParams
);

// get bmp image data 
std::vector<uint8_t> getBMPImageData(const std::wstring& filename, WindowInfo* gifParams);


#endif // CAPTURE_H
