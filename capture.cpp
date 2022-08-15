// capture.cpp 

#include "capture.hh"    // function declarations

// probably should convert to non-namespace later 
using namespace Gdiplus;

// convert an integer to string 
std::string intToString(int i){
    std::stringstream ss;
    ss << i;
    std::string i_str = ss.str();
    return i_str;
}

int getEncoderClsid(const WCHAR* format, CLSID* pClsid){
    UINT num = 0;          // number of image encoders
    UINT size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if(size == 0){
        return -1;  // Failure
    }

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if(pImageCodecInfo == NULL){
        return -1;  // Failure
    }

    GetImageEncoders(num, size, pImageCodecInfo);

    for(UINT j = 0; j < num; ++j){
        if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 ){
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }    
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

void bitmapToBMP(HBITMAP hbmpImage, int width, int height, std::string filename){
    // with unique ptr we don't have to worry about manually deleting the bitmap
    std::unique_ptr<Gdiplus::Bitmap> p_bmp = std::unique_ptr<Gdiplus::Bitmap>(Gdiplus::Bitmap::FromHBITMAP(hbmpImage, NULL));
    
    CLSID pngClsid;
    
    // creating BMP images
    int result = getEncoderClsid(L"image/bmp", &pngClsid);  
    if(result != -1){
        std::cout << "Encoder succeeded" << std::endl;
    }else{
        std::cout << "Encoder failed" << std::endl;
    }
    
    // convert filename to a wstring first
    std::wstring fname = std::wstring(filename.begin(), filename.end());
    
    // use .c_str to convert to wchar_t*
    p_bmp->Save(fname.c_str(), &pngClsid, NULL);
}

bool ptIsInRange(POINT& start, int width, int height, POINT& pt){
    return (pt.x >= start.x && pt.x <= start.x + width && pt.y >= start.y && pt.y <= start.y + height);
}

bool screenCapture(int x, int y, int width, int height, const char* filename, bool getCursor){
    HDC hDc = CreateCompatibleDC(0);
    HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);
    SelectObject(hDc, hBmp);
    BitBlt(hDc, 0, 0, width, height, GetDC(0), x, y, SRCCOPY);
    
    // capture the cursor and add to screenshot if so desired
    if(getCursor){
        CURSORINFO screenCursor{sizeof(screenCursor)};
        GetCursorInfo(&screenCursor);
        if(screenCursor.flags == CURSOR_SHOWING){
            RECT rcWnd;
            HWND hwnd = GetDesktopWindow();
            GetWindowRect(hwnd, &rcWnd);
            ICONINFO iconInfo{sizeof(iconInfo)};
            GetIconInfo(screenCursor.hCursor, &iconInfo);
            int cursorX = screenCursor.ptScreenPos.x - iconInfo.xHotspot - x;
            int cursorY = screenCursor.ptScreenPos.y - iconInfo.yHotspot - y;
            BITMAP cursorBMP = {0};
            GetObject(iconInfo.hbmColor, sizeof(cursorBMP), &cursorBMP);
            DrawIconEx(
                hDc, 
                cursorX, 
                cursorY, 
                screenCursor.hCursor, 
                cursorBMP.bmWidth, 
                cursorBMP.bmHeight, 
                0, 
                NULL, 
                DI_NORMAL
            );
        }
    }
    
    bitmapToBMP(hBmp, width, height, filename);
    DeleteObject(hBmp);
    return true;
}

// notice this takes a function pointer!
void getSnapshots(
    int nImages, 
    int delay, 
    int x, 
    int y, 
    int width, 
    int height, 
    std::vector<uint8_t> (*filter)(const std::string&, WindowInfo*), 
    WindowInfo* gifParams
){
    HWND mainWindow = gifParams->mainWindow;
    
    // Initialize GDI+.
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // get temp directory 
    std::string dirName = gifParams->tempDirectory;

    std::string name;
    for(int i = 0; i < nImages; i++){
        // put all images in temp folder
        name = dirName + "/screen" + intToString(i) + ".bmp";
        screenCapture(x, y, width, height, name.c_str(), gifParams->getCursor);
        Sleep(delay);
    }
    
    GdiplusShutdown(gdiplusToken);
}

// this function resizes bmp images. it's used to make sure all frames being fed to the gif generator 
// are the same dimension. this occurs when a user specifies a directory of bmps to generate a gif from.
// takes in a number indicating how many images to check for resize, and a width and height to resize to
// it returns an integer indicating if anything was resized (1 = something was resized);
// for now, create a new folder called temp_resized to store this new set of images (including the ones that weren't resized)
// last argument is captionText, which is a string that, if not empty (""), will be written near the bottom of each frame  
int resizeBMPs(int nImages, std::vector<std::string>& images, int width, int height, std::string& captionText){
    int resizeResult = 0;
    
    // initialize gdiplus 
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // make a temp_resized directory 
    std::wstring dirName = L"temp_resized";
    if(CreateDirectory(dirName.c_str(), NULL)){
        // do nothing
    }else if(ERROR_ALREADY_EXISTS == GetLastError()){
        // if it exists, empty out the directory
    }else{
        // directory couldn't be made
    }
    
    for(int i = 0; i < nImages; i++){
        std::string filename = images[i];
        std::wstring wstr = std::wstring(filename.begin(), filename.end());
        const wchar_t *widestr = wstr.c_str();
        
        Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(widestr, false);
        int h = bmp->GetHeight();
        int w = bmp->GetWidth();
        
        CLSID pngClsid;
        
        // if dimensions of current image match the initial image and no caption text, just skip this one 
        // but add it to the new temp directory
        if(h == height && w == width && captionText == ""){
            int result = getEncoderClsid(L"image/bmp", &pngClsid);
            if(result == -1){
                std::cout << "Encoder failed" << std::endl;
            }
            
            filename = "temp_resized/screen" + intToString(i) + ".bmp";
            std::wstring fname = std::wstring(filename.begin(), filename.end());
            bmp->Save(fname.c_str(), &pngClsid, NULL);
            delete bmp;
            continue;
        }
        
        // make a new empty bmp with the new dimensions
        Gdiplus::Bitmap* newBMP = new Gdiplus::Bitmap(width, height, bmp->GetPixelFormat());
        
        // resize the original bmp
        Gdiplus::Graphics graphics(newBMP); // the new bitmap is the new canvas to draw the resized image on 
        graphics.DrawImage(bmp, 0, 0, width, height);
    
        // caption if there's text in the specified box 
        if(captionText != ""){
            std::wstring mtext = std::wstring(captionText.begin(), captionText.end());
            const wchar_t* string = mtext.c_str(); //L"BLAH BLAH BLAH";
            int stringLen = mtext.size();
            
            // decide where to place the text, x-coordinate-wise 
            // assume each char in the string takes up 15 pixels?
            int xCoord = (w/2) - ((stringLen*15)/2);
            
            Gdiplus::FontFamily impactFont(L"Impact");
            Gdiplus::StringFormat strFormat;
            Gdiplus::GraphicsPath gpath;       // use this to hold the outline of the string we want to draw 
            gpath.AddString(
                string,                        // the string
                wcslen(string),                // length of string
                &impactFont,                   // font family
                FontStyleRegular,              // style of type face 
                32,                            // font size 
                Point(xCoord, (h/2 + h/3)),    // where to put the string 
                &strFormat                     // layout information for the string 
            );
            
            Gdiplus::Pen pen(Color(0,0,0), 2); // color and width of pen 
            pen.SetLineJoin(LineJoinRound);    // prevent sharp pointers from occurring on some chars 
            graphics.SetSmoothingMode(SmoothingModeAntiAlias); // antialias the text so the outline doesn't look choppy
            graphics.DrawPath(&pen, &gpath);
            
            Gdiplus::SolidBrush brush(Color(255,255,255,255));
            graphics.FillPath(&brush, &gpath);
        }
        
        // overwite old file with this new one
        int result = getEncoderClsid(L"image/bmp", &pngClsid);
        if(result != -1){
            //std::cout << "Encoder succeeded" << std::endl;
        }else{
            std::cout << "Encoder failed" << std::endl;
        }
        
        // convert filename to a wstring first
        //filename = filename.substr(0, filename.size() - 4);
        filename = "temp_resized/screen" + intToString(i) + ".bmp";
        std::wstring fname = std::wstring(filename.begin(), filename.end());
        
        newBMP->Save(fname.c_str(), &pngClsid, NULL);
        //std::cout << "status: " << newBMP->Save(fname.c_str(), &pngClsid, NULL) << std::endl;
        
        resizeResult = 1;
        
        //delete bmp;
        delete newBMP;
    }
    
    // shutdown gdiplus 
    Gdiplus::GdiplusShutdown(gdiplusToken);
    
    return resizeResult;
}

/***

    get bmp image data 
    also applies a filter to images if needed 

***/
// get a bmp image and extract the image data into a uint8_t array 
// which will be passed to gif functions from gif.h to create the gif 
std::vector<uint8_t> getBMPImageData(const std::string& filename, WindowInfo* gifParams){
    std::wstring filtername = (gifParams->filters)[gifParams->selectedFilter];
    
    // bmps have a 54 byte header 
    static constexpr size_t HEADER_SIZE = 54;
    
    // read in bmp file as stream
    std::ifstream bmp(filename, std::ios::binary);
    
    // this represents the header of the bmp file 
    std::array<char, HEADER_SIZE> header;
    
    // read in 54 bytes of the file and put that data in the header array
    bmp.read(header.data(), header.size());
    
    //auto fileSize = *reinterpret_cast<uint32_t *>(&header[2]);
    auto dataOffset = *reinterpret_cast<uint32_t *>(&header[10]);
    auto width = *reinterpret_cast<uint32_t *>(&header[18]);
    auto height = *reinterpret_cast<uint32_t *>(&header[22]);
    auto depth = *reinterpret_cast<uint16_t *>(&header[28]);
    
    // now get the image pixel data
    // not sure this part is necessary since dataOffset comes out to be 54 
    // which is the header size, when I test it
    std::vector<char> img(dataOffset - HEADER_SIZE);
    bmp.read(img.data(), img.size());
    
    // use this vector to store all the pixel data, which will be returned
    std::vector<uint8_t> finalImageData;
    
    if((int)depth == 24){
        // since 24-bit bmps round up to nearest width divisible by 4, 
        // there might be some extra padding at the end of each pixel row 
        int paddedWidth = (int)width*3;
        
        while(paddedWidth%4 != 0){
            paddedWidth++;
        }
        
        // find out how much padding there is per row 
        int padding = paddedWidth - ((int)width*3);
        
        // figure out the size of the pixel data, which includes the padding 
        auto dataSize = (3*width*height) + (height*padding);
        img.resize(dataSize);
        bmp.read(img.data(), img.size());

        int RGBcounter = 0;
        int widthCount = 0;
        
        std::vector<uint8_t> image;
        
        // add in the alpha channel to the data 
        for(int i = 0; i < (int)dataSize; i++){
            image.push_back(img[i]);
            RGBcounter++;
        
            // after every third element, add a 255 (this is for the alpha channel)
            if(RGBcounter == 3){
                image.push_back(255);
                RGBcounter = 0;
            }
            
            widthCount++;
            
            // check if we've already gotten all the color channels for a row (if so, skip the padding!)
            if(widthCount == ((int)width*3)){
                widthCount = 0;
                i += padding;
            }
        }

        // then swap the blue and red channels so we get RGBA
        for(int i = 0; i <= (int)image.size() - 4; i += 4){
            char temp = image[i];
            image[i] = image[i+2];
            image[i+2] = temp;
        }
        
        int widthSize = 4*(int)width; // total num channels per row 
        std::vector<uint8_t> image2;
        
        for(int j = (int)image.size() - 1; j >= 0; j -= widthSize){
            for(int k = widthSize - 1; k >= 0; k--){
                image2.push_back((uint8_t)image[j - k]);
            }
        }
        
        finalImageData = image2;
    }else if((int)depth == 32){
        // width*4 because each pixel is 4 bytes (32-bit bmp)
        // ((width*4 + 3) & (~3)) * height; -> this uses bit masking to get the width as a multiple of 4
        auto dataSize = ((width*4 + 3) & (~3)) * height;
        img.resize(dataSize);
        bmp.read(img.data(), img.size());
        
        // need to swap R and B (img[i] and img[i+2]) so that the sequence is RGBA, not BGRA
        // also, notice that each pixel is represented by 4 bytes, not 3, because
        // the bmp images are 32-bit
        for(int i = 0; i <= (int)(dataSize - 4); i += 4){
            char temp = img[i];
            img[i] = img[i+2];
            img[i+2] = temp;
        }
        
        // change char vector to uint8_t vector (why is this necessary, if at all?)
        // be careful! bmp image data is stored upside-down and flipped horizontally :<
        // so traverse backwards, but also, for each row, invert the row also!
        std::vector<uint8_t> image;
        int widthSize = 4 * (int)width;
        for(int j = (dataSize - 1); j >= 0; j -= widthSize){
            for(int k = widthSize - 1; k >= 0; k--){
                image.push_back((uint8_t)img[j - k]);
            }
        }
        
        finalImageData = image;
    }else{
        // return an empty vector 
        return finalImageData;
    }
    
    // apply filters as needed 
    if(filtername == L"none"){
        return finalImageData;
    }

    // use gifParams to get specific parameters for specific filters
    if(filtername == L"inverted")     inversionFilter(finalImageData);
    if(filtername == L"saturated")    saturationFilter(gifParams->saturationValue, finalImageData);
    if(filtername == L"weird")        weirdFilter(finalImageData);
    if(filtername == L"grayscale")    grayscaleFilter(finalImageData);
    if(filtername == L"edge_detect")  edgeDetectionFilter(finalImageData, (int)width, (int)height);
    if(filtername == L"mosaic")       mosaicFilter(finalImageData, (int)width, (int)height, gifParams->mosaicChunkSize);
    if(filtername == L"outline")      outlineFilter(finalImageData, (int)width, (int)height, gifParams->outlineColorDiffLimit);
    if(filtername == L"voronoi")      voronoiFilter(finalImageData, (int)width, (int)height, gifParams->voronoiNeighborConstant);
    if(filtername == L"blur")         blurFilter(finalImageData, (int)width, (int)height, (double)gifParams->blurFactor); // TODO: just change blur factor to int?
    
    return finalImageData;
}

