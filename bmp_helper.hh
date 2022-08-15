#ifndef BMP_HELPER_H
#define BMP_HELPER_H

#include <iostream> // for reading in data (includes ios)
#include <fstream>  // for reading in bmp image data
#include <array>    // for reading in bmp image data
#include <utility>  // for std::pair
#include <vector>
#include <string>
#include <cmath>	// for using sqrt, floor
#include <ctime>	// for time() 
#include <cstdlib>	// for rand() and srand()

#include "voronoi_helper.hh"

// correct channel value if > 255 or < 0
int correctRGB(int channel);

// get height and width of bmp image 
std::vector<int> getBMPHeightWidth(const std::string filename);

// get the pixel coordinates given an r channel index of a pixel in an array of image data 
// that is arranged like r,g,b,a,r,g,b,a,... 
std::pair<int, int> getPixelCoords(int index, int width, int height);

// filters 
// TODO: move these to a separate file?
void inversionFilter(std::vector<uint8_t>& imageData);
void saturationFilter(float saturationVal, std::vector<uint8_t>& imageData);
void weirdFilter(std::vector<uint8_t>& imageData);
void grayscaleFilter(std::vector<uint8_t>& imageData);
void edgeDetectionFilter(std::vector<uint8_t>& imageData, int width, int height);
void mosaicFilter(std::vector<uint8_t>& imageData, int width, int height, int chunkSize);
void outlineFilter(std::vector<uint8_t>& imageData, int width, int height, int colorDiffLimit);
void voronoiFilter(std::vector<uint8_t>& imageData, int width, int height, int neighborConstant);

std::vector<double> generateGaussBoxes(double stdDev, double numBoxes);
void boxBlurHorz(std::vector<uint8_t>& src, std::vector<uint8_t>& trgt, int width, int height, double stdDev);
void boxBlurTotal(std::vector<uint8_t>& src, std::vector<uint8_t>& trgt, int width, int height, double stdDev);
void gaussBlur(std::vector<uint8_t>& src, std::vector<uint8_t>& trgt, int width, int height, double stdDev);
void blurFilter(std::vector<uint8_t>& imageData, int width, int height, double blurFactor);

#endif 