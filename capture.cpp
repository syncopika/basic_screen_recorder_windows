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
    std::vector<uint8_t>(*filter)(const std::wstring&, WindowInfo*),
    WindowInfo* captureParams
){
    HWND mainWindow = captureParams->mainWindow;

    // Initialize GDI+.
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // get temp directory 
    std::string dirName = captureParams->tempDirectory;

    std::string name;
    for(int i = 0; i < nImages; i++){
        // put all images in temp folder
        name = dirName + "/screen" + intToString(i) + ".bmp";
        screenCapture(x, y, width, height, name.c_str(), captureParams->getCursor);
        Sleep(delay);
    }

    // apply filter and caption as needed
    if(captureParams->selectedFilter != 0 || captureParams->captionText != L""){
        std::wstring dname = std::wstring(dirName.begin(), dirName.end());

        for(int i = 0; i < nImages; i++){
            std::wstring nextFrame = dname + L"/screen" + std::to_wstring(i) + L".bmp";

            // post message to indicate which frame is being processed
            std::cout << "processing frame: " << i << "\n";
            PostMessage(mainWindow, ID_PROCESS_FRAME, (WPARAM)i, 0);

            Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(nextFrame.c_str(), false);
            int height = bmp->GetHeight();
            int width = bmp->GetWidth();

            CLSID bmpClsid;

            // apply filter
            if(captureParams->selectedFilter != 0){
                std::vector<uint8_t> img = (*filter)(nextFrame, captureParams);

                //std::cout << "image height: " << h << "\n";
                //std::cout << "image width: " << w << "\n";
                //std::cout << "image data length: " << img.size() << "\n";

                // need to cycle through the pixels of bmp to edit
                for(int j = 0; j < height; j++){
                    for(int k = 0; k < width; k++){
                        int r = img[(4 * j * width) + (4 * k)];
                        int g = img[(4 * j * width) + (4 * k) + 1];
                        int b = img[(4 * j * width) + (4 * k) + 2];
                        int alpha = img[(4 * j * width) + (4 * k) + 3];
                        bmp->SetPixel(k, j, (Color::MakeARGB(alpha, r, g, b)));
                    }
                }
            }

            // copy over to another new bitmap
            // make a copy because having trouble overwriting the existing file before deleting the Bitmap pointer
            // maybe helpful for explaining: https://stackoverflow.com/questions/1036115/overwriting-an-image-using-save-method-of-bitmap
            Gdiplus::Bitmap* newBMP = new Gdiplus::Bitmap(width, height, bmp->GetPixelFormat());
            Gdiplus::Graphics graphics(newBMP);
            graphics.DrawImage(bmp, 0, 0, width, height);

            // apply caption
            if(captureParams->captionText != L""){
                std::wstring mtext = std::wstring(captureParams->captionText.begin(), captureParams->captionText.end());
                const wchar_t* string = mtext.c_str(); //L"BLAH BLAH BLAH";
                int stringLen = mtext.size();

                // decide where to place the text, x-coordinate-wise
                // assume each char in the string takes up 15 pixels?
                int xCoord = (width / 2) - ((stringLen * 15) / 2);

                Gdiplus::FontFamily impactFont(L"Impact");
                Gdiplus::StringFormat strFormat;
                Gdiplus::GraphicsPath gpath;       // use this to hold the outline of the string we want to draw
                gpath.AddString(
                    string,                        // the string
                    wcslen(string),                // length of string
                    &impactFont,                   // font family
                    FontStyleRegular,              // style of type face
                    32,                            // font size
                    Point(xCoord, (height / 2 + height / 3)),    // where to put the string
                    &strFormat                     // layout information for the string
                );

                Gdiplus::Pen pen(Color(0, 0, 0), 2); // color and width of pen
                pen.SetLineJoin(LineJoinRound);    // prevent sharp pointers from occurring on some chars
                graphics.SetSmoothingMode(SmoothingModeAntiAlias); // antialias the text so the outline doesn't look choppy
                graphics.DrawPath(&pen, &gpath);

                Gdiplus::SolidBrush brush(Color(255, 255, 255, 255));
                graphics.FillPath(&brush, &gpath);
            }

            delete bmp;

            bool deleteStatus = DeleteFile(nextFrame.c_str());
            //std::cout << "delete status: " << deleteStatus << "\n";

            int result = getEncoderClsid(L"image/bmp", &bmpClsid);
            if(result != -1){
                //std::cout << "Encoder succeeded" << std::endl;
            }else{
                std::cout << "Encoder failed" << std::endl;
            }

            Gdiplus::Status s = newBMP->Save(nextFrame.c_str(), &bmpClsid, NULL);
            //std::cout << "frame save status: " << s << "\n";

            delete newBMP;
        }
    }

    // shutdown gdiplus 
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

/***

    get bmp image data 
    also applies a filter to images if needed 

***/
// get a bmp image and extract the image data into a uint8_t array 
// which will be passed to gif functions from gif.h to create the gif 
std::vector<uint8_t> getBMPImageData(const std::wstring& filename, WindowInfo* gifParams){
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

