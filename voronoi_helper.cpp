#include "voronoi_helper.hh"

// get distance between 2 points
// using euclidean distance formula
// however, we don't really need the sqrt when used in getting nearest neighbors since we only want to find the minimum distance
float getDist(int x1, int x2, int y1, int y2){
    return (float)((x1 - x2) * (x1 - x2)) + ((y1 - y2) * (y1 - y2));
}

// check if a Node is a leaf (both children are null)
bool isLeaf(Node* n){
    return n->left == nullptr && n->right == nullptr;
}

// recursive helper function to delete nodes of tree used in deleteTree
void deleteHelper(Node* root){
    if(root == nullptr){
        return;
    }
    
    deleteHelper(root->left);
    deleteHelper(root->right);
    root->left = nullptr;
    root->right = nullptr;
    delete root;
}

// delete all the nodes of a tree given the root 
void deleteTree(Node*& root){
    deleteHelper(root);
    root = nullptr;
}


// returns the root of the resulting 2d tree 
// in this use case our dimensions will be x and y (since each pixel has an x,y coordinate), so only 2 dimensions 
Node* build2dTree(std::vector<CustomPoint> pointsList, int currDim){
    int maxDim = 2; // ensures dimension is either 0 or 1
    
    // base cases
    if(pointsList.size() == 0){
        return nullptr;
    }
    
    if(pointsList.size() == 1){
        Node* newNode = new Node();
        newNode->data = {pointsList[0].x, pointsList[0].y};
        newNode->dim = currDim;
        newNode->point = pointsList[0];
        return newNode;
    }
    
    if(pointsList.size() == 2){
        // since it's a BST, the 2nd element (at index 1) will be larger and thus the parent of 
        // the 1st element, which will go to the left of the parent
        Node* newParent = new Node();
        newParent->data = {pointsList[1].x, pointsList[1].y};
        newParent->dim = currDim;
        newParent->point = pointsList[1];
        
        Node* newChild = new Node();
        newChild->data = {pointsList[0].x, pointsList[0].y};
        newChild->dim = (currDim + 1) % maxDim;
        newChild->point = pointsList[0];
        
        newParent->left = newChild;
        
        return newParent;
    }
    
    // sort the current list in ascending order depending on the current dimension
    char dim = (currDim == 0) ? 'x' : 'y';
    std::sort(pointsList.begin(), pointsList.end(), [dim](const CustomPoint a, const CustomPoint b){
        if(dim == 'x'){
            return a.x < b.x;
        }else{
            return a.y < b.y;
        }
    });
    
    // take the median point and set as new node
    int midIndex = (int)floor((pointsList.size() - 1) / 2);
    Node* newNode = new Node();
    newNode->data = {pointsList[midIndex].x, pointsList[midIndex].y};
    newNode->dim = currDim;
    newNode->point = pointsList[midIndex];
    
    // split the points list into two parts and pass each part to each subtree
    std::vector<CustomPoint> leftHalf; //= std::vector<CustomPoint>(pointsList.begin(), pointsList.begin() + midIndex);
    for(int i = 0; i < midIndex; i++){
        leftHalf.push_back(pointsList[i]);
    }
    
    std::vector<CustomPoint> rightHalf; //= std::vector<CustomPoint>(pointsList.begin() + midIndex + 1, pointsList.end());
    for(int j = midIndex + 1; j < (int)pointsList.size(); j++){
        rightHalf.push_back(pointsList[j]);
    }
    
    newNode->left = build2dTree(leftHalf, (currDim + 1) % maxDim);
    newNode->right = build2dTree(rightHalf, (currDim + 1) % maxDim);
    
    return newNode;
}

// recursive helper function for finding nearest neighbor of a given point's x and y coords 
void findNearestNeighborHelper(Node* root, CustomPoint& nearestNeighbor, float& minDist, int x, int y){
    float currDist = getDist(root->data.first, x, root->data.second, y);
    
    if(isLeaf(root)){
        if(currDist < minDist){
            nearestNeighbor = root->point;
            minDist = currDist;
        }
    }else{
        if(currDist < minDist){
            nearestNeighbor = root->point;
            minDist = currDist;
        }
        
        if(root->left && !root->right){
            // if a node has one child, it will always be the left child
            findNearestNeighborHelper(root->left, nearestNeighbor, minDist, x, y);
        }else{
            // find the right direction to go in the tree based on dimension //distance
            int currDimToCompare = (root->dim == 0) ? x : y;
            
            if(currDimToCompare == x){
                // is x greater than the current node's x? if so, we want to go right. else left.
                if(x > root->data.first){
                    findNearestNeighborHelper(root->right, nearestNeighbor, minDist, x, y);
                    
                    // then, we check if the other subtree might actually have an even closer neighbor!
                    if(x - minDist < root->data.first){
                        findNearestNeighborHelper(root->left, nearestNeighbor, minDist, x, y);
                    }
                }else{
                    findNearestNeighborHelper(root->left, nearestNeighbor, minDist, x, y);
                    if(x + minDist > root->data.first){
                        // x + record.minDist forms a circle. the circle in this case
                        // encompasses this current node's coordinates, so we can get closer to the query node
                        // by checking the right subtree
                        findNearestNeighborHelper(root->right, nearestNeighbor, minDist, x, y);
                    }
                }
            }else{
                if(y > root->data.second){
                    findNearestNeighborHelper(root->right, nearestNeighbor, minDist, x, y);
                    if(y - minDist < root->data.second){
                        findNearestNeighborHelper(root->left, nearestNeighbor, minDist, x, y);
                    }
                }else{
                    findNearestNeighborHelper(root->left, nearestNeighbor, minDist, x, y);
                    if(y + minDist > root->data.second){
                        findNearestNeighborHelper(root->right, nearestNeighbor, minDist, x, y);
                    }
                }
            }
        }
    }
}

// find nearest neighbor in 2d tree given a point's x and y coords and the tree's root 
CustomPoint findNearestNeighbor(Node* root, int x, int y){
    // set default values
    CustomPoint nearestNeighbor = root->point;
    float minDist = getDist(root->data.first, x, root->data.second, y);
    
    findNearestNeighborHelper(root, nearestNeighbor, minDist, x, y);
    
    return nearestNeighbor;
}