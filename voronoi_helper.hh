#ifndef VORONOI_HELPER_H
#define VORONOI_HELPER_H

/*** 
	
	2d tree (for finding nearest neighbors in image data consisting of x and y dimensions)

***/
#include <iostream>
#include <cmath>
#include <algorithm>
#include <utility>
#include <vector>

// Point struct
// represents the position and color of a pixel 
struct CustomPoint {
	int x;
	int y;
	unsigned char r;
	unsigned char g;
	unsigned char b;
};


// each node takes Point info (x, y, rgb) and a dimension (0 for 'x', 1 for 'y')
struct Node {
	std::pair<int, int> data; // [point.x, point.y];
	CustomPoint point;
	int dim;
	Node* left = nullptr;
	Node* right = nullptr;
};

// get distance between 2 points
// using euclidean distance formula
// however, we don't really need the sqrt when used in getting nearest neighbors since we only want to find the minimum distance
float getDist(int x1, int x2, int y1, int y2);

// check if a Node is a leaf (both children are null)
bool isLeaf(Node* n);

// recursive helper function to delete nodes of tree used in deleteTree
void deleteHelper(Node* root);

// delete all the nodes of a tree given the root 
void deleteTree(Node*& root);

// returns the root of the resulting 2d tree 
// in this use case our dimensions will be x and y (since each pixel has an x,y coordinate), so only 2 dimensions 
Node* build2dTree(std::vector<CustomPoint> pointsList, int currDim);

// recursive helper function for finding nearest neighbor of a given point's x and y coords 
void findNearestNeighborHelper(Node* root, CustomPoint& nearestNeighbor, float& minDist, int x, int y);

// find nearest neighbor in 2d tree given a point's x and y coords and the tree's root 
CustomPoint findNearestNeighbor(Node* root, int x, int y);

#endif 
