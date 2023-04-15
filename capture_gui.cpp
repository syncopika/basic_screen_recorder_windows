#include "framework.h"
#include "capture_gui.hh"
#include <shlobj.h>

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

/*****************

    global variables

*****************/
int x1Pos = 0; // top left x coord 
int y1Pos = 0; // top left y coord
int x2Pos = GetSystemMetrics(SM_CXSCREEN); // screen bottom right x coord 
int y2Pos = GetSystemMetrics(SM_CYSCREEN); // screen bottom right y coord 

// for selection window 
bool bDrag = false;
bool bDraw = false;

// point structs for keeping track of start and final coordinates
POINT ptCurr = {0, 0};
POINT ptNew = {0, 0};

// register 4 different windows 
const wchar_t g_szClassName[]  = L"mainGUI";
const wchar_t g_szClassName2[] = L"mainPage";
const wchar_t g_szClassName3[] = L"parametersPage";
const wchar_t g_szClassName4[] = L"selectionWindow";
const wchar_t g_szClassName5[] = L"aboutPage";

// handler variables for the windows 
HWND hwnd;    // this is the main GUI window handle (the parent window of the main and parameter pages)
HWND mainPage; // this is the main page of the GUI 
HWND parameterPage; // this is the window handle for the page where the user can set some parameters 
HWND selectionWindow; // this is the handle for the rubber-banding selection window 
HWND aboutPage; // an about page

// use Tahoma font for the text 
// this HFONT object needs to be deleted (via DeleteObject) when program ends 
HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
      OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
      DEFAULT_PITCH | FF_DONTCARE, TEXT("Tahoma"));

// for WASAPI capturer
bool DisableMMCSS;

IMMDeviceEnumerator* pEnumerator = NULL;
IMMDevice* pDevice = NULL;
IAudioClient* pAudioClient = NULL;
IAudioCaptureClient* pCaptureClient = NULL;
WAVEFORMATEX* pwfx = NULL;

// filters map - order matters!
std::map<int, std::wstring> filterMap = {
    {0, L"none"},
    {1, L"inverted"},
    {2, L"saturated"},
    {3, L"weird"},
    {4, L"grayscale"},
    {5, L"edge_detect"},
    {6, L"mosaic"},
    {7, L"outline"},
    {8, L"voronoi"},
    {9, L"blur"}
};

// default settings 
static WindowInfo captureParams; // WindowInfo is from capture.hh
static WASAPICapturerInfo audioCaptureInfo;

std::string floatToString(float f){
    std::ostringstream out;
    out.precision(2);
    out << std::fixed << f;
    return out.str();
}

// TODO: move this elsewhere pls
// from: https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/multimedia/audio/CaptureSharedTimerDriven/WASAPICaptureSharedTimerDriven.cpp
//
//  WAV file writer.
//
//  This is a VERY simple .WAV file writer.
//

//
//  A wave file consists of:
//
//  RIFF header:    8 bytes consisting of the signature "RIFF" followed by a 4 byte file length.
//  WAVE header:    4 bytes consisting of the signature "WAVE".
//  fmt header:     4 bytes consisting of the signature "fmt " followed by a WAVEFORMATEX 
//  WAVEFORMAT:     <n> bytes containing a waveformat structure.
//  DATA header:    8 bytes consisting of the signature "data" followed by a 4 byte file length.
//  wave data:      <m> bytes containing wave data.
//
//
//  Header for a WAV file - we define a structure describing the first few fields in the header for convenience.
//
struct WAVEHEADER {
    DWORD   dwRiff;                     // "RIFF"
    DWORD   dwSize;                     // Size
    DWORD   dwWave;                     // "WAVE"
    DWORD   dwFmt;                      // "fmt "
    DWORD   dwFmtSize;                  // Wave Format Size
};

//  Static RIFF header, we'll append the format to it.
const BYTE WaveHeader[] = {
    'R','I','F','F',0x00,0x00,0x00,0x00,'W','A','V','E','f','m','t',' ', 0x00, 0x00, 0x00, 0x00
};

//  Static wave DATA tag.
const BYTE WaveData[] = {'d','a','t','a'};

//
//  Write the contents of a WAV file.  We take as input the data to write and the format of that data.
//
bool WriteWaveFile(HANDLE FileHandle, const BYTE* Buffer, const size_t BufferSize, const WAVEFORMATEX* WaveFormat){
    std::cout << "writing wav. buffer size of data: " << BufferSize << "\n";

    DWORD waveFileSize = sizeof(WAVEHEADER) + sizeof(WAVEFORMATEX) + WaveFormat->cbSize + sizeof(WaveData) + sizeof(DWORD) + static_cast<DWORD>(BufferSize);
    BYTE* waveFileData = new BYTE[waveFileSize];
    BYTE* waveFilePointer = waveFileData;
    WAVEHEADER* waveHeader = reinterpret_cast<WAVEHEADER*>(waveFileData);

    if (waveFileData == NULL){
        printf("Unable to allocate %d bytes to hold output wave data\n", waveFileSize);
        return false;
    }

    //
    //  Copy in the wave header - we'll fix up the lengths later.
    //
    CopyMemory(waveFilePointer, WaveHeader, sizeof(WaveHeader));
    waveFilePointer += sizeof(WaveHeader);

    //
    //  Update the sizes in the header.
    //
    waveHeader->dwSize = waveFileSize - (2 * sizeof(DWORD));
    waveHeader->dwFmtSize = sizeof(WAVEFORMATEX) + WaveFormat->cbSize;

    //
    //  Next copy in the WaveFormatex structure.
    //
    CopyMemory(waveFilePointer, WaveFormat, sizeof(WAVEFORMATEX) + WaveFormat->cbSize);
    waveFilePointer += sizeof(WAVEFORMATEX) + WaveFormat->cbSize;

    //
    //  Then the data header.
    //
    CopyMemory(waveFilePointer, WaveData, sizeof(WaveData));
    waveFilePointer += sizeof(WaveData);
    *(reinterpret_cast<DWORD*>(waveFilePointer)) = static_cast<DWORD>(BufferSize);
    waveFilePointer += sizeof(DWORD);

    //
    //  And finally copy in the audio data.
    //
    CopyMemory(waveFilePointer, Buffer, BufferSize);

    //
    //  Last but not least, write the data to the file.
    //
    DWORD bytesWritten;
    if(!WriteFile(FileHandle, waveFileData, waveFileSize, &bytesWritten, NULL)){
        printf("Unable to write wave file: %d\n", GetLastError());
        delete[] waveFileData;
        return false;
    }

    if(bytesWritten != waveFileSize){
        printf("Failed to write entire wave file\n");
        delete[] waveFileData;
        return false;
    }
    delete[] waveFileData;
    return true;
}

//
//  Write the captured wave data to an output file so that it can be examined later.
//
void SaveWaveData(BYTE* CaptureBuffer, size_t BufferSize, const WAVEFORMATEX* WaveFormat, std::string fileName){
    wchar_t waveFileName[MAX_PATH];
    std::cout << "filename of wav: " << fileName << "\n";
    std::wstring fName = std::wstring(fileName.begin(), fileName.end()) + L".wav"; // TODO: is this ok?
    //HRESULT hr = StringCbCopy(waveFileName, sizeof(waveFileName), L"WASAPICaptureTimerDriven-");
    HRESULT hr = StringCbCopy(waveFileName, sizeof(waveFileName), fName.c_str());

    if(SUCCEEDED(hr)){
        HANDLE waveHandle = CreateFile(waveFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        if(waveHandle != INVALID_HANDLE_VALUE){
            if (WriteWaveFile(waveHandle, CaptureBuffer, BufferSize, WaveFormat))
            {
                printf("Successfully wrote WAVE data to %S\n", waveFileName);
            }
            else
            {
                printf("Unable to write wave file\n");
            }
            CloseHandle(waveHandle);
        }else{
            printf("Unable to open output WAV file %S: %d\n", waveFileName, GetLastError());
        }
    }
}

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

void setUpForAudioCollection(
    IMMDeviceEnumerator*& pEnumerator,
    IMMDevice*& pDevice,
    IAudioClient*& pAudioClient,
    IAudioCaptureClient*& pCaptureClient,
    WAVEFORMATEX*& pwfx
){
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**)&pEnumerator
    );
    std::cout << "device enumerator is set up" << std::endl;

    hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender,
        eConsole,
        &pDevice
    );
    std::cout << "got default audio endpoint" << std::endl;

    hr = pDevice->Activate(
        IID_IAudioClient,
        CLSCTX_ALL,
        NULL,
        (void**)&pAudioClient
    );
    std::cout << "audio client is set up" << std::endl;

    hr = pAudioClient->GetMixFormat(&pwfx);
    std::cout << "got mix format" << std::endl;

    // https://stackoverflow.com/questions/64318206/record-an-audio-stream-with-wasapi
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0,
        0,
        pwfx,
        NULL
    );
    std::cout << "audio client initialized" << std::endl;

    hr = pAudioClient->GetService(
        IID_IAudioCaptureClient,
        (void**)&pCaptureClient
    );
}

/***

    functions to make creating window elements easier

***/
void createEditBox(
    std::wstring defaultText,
    int width,
    int height,
    int xCoord,
    int yCoord,
    HWND parent,
    HINSTANCE hInstance,
    HMENU elementId,
    HFONT hFont
){
    HWND editBox = CreateWindowW(
        TEXT("edit"),
        defaultText.c_str(),
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        xCoord, yCoord,  /* x, y coords */
        width, height, /* width, height */
        parent,
        elementId,
        hInstance,
        NULL
    );
    SendMessage(editBox, WM_SETFONT, (WPARAM)hFont, true);
}

void createLabel(
    std::wstring defaultText,
    int width,
    int height,
    int xCoord,
    int yCoord,
    HWND parent,
    HINSTANCE hInstance,
    HMENU elementId,
    HFONT hFont
){
    HWND label = CreateWindow(
        TEXT("STATIC"),
        defaultText.c_str(),
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        xCoord, yCoord,  /* x, y coords */
        width, height, /* width, height */
        parent,
        elementId,
        hInstance,
        NULL
    );
    SendMessage(label, WM_SETFONT, (WPARAM)hFont, true);
}

void createCheckBox(
    std::wstring defaultText, 
    int width, 
    int height, 
    int xCoord, 
    int yCoord, 
    HWND parent, 
    HINSTANCE hInstance, 
    HMENU elementId, 
    HFONT hFont
){
    HWND checkBox = CreateWindow(
        TEXT("button"),
        defaultText.c_str(),
        BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE,
        xCoord, yCoord,  /* x, y coords */
        width, height, /* width, height */
        parent,
        elementId,
        hInstance,
        NULL
    );
    SendMessage(checkBox, WM_SETFONT, (WPARAM)hFont, true);
}

void createRadioButton(
    std::wstring defaultText,
    int width,
    int height,
    int xCoord,
    int yCoord,
    HWND parent,
    HINSTANCE hInstance,
    HMENU elementId,
    HFONT hFont,
    bool checked
) {
    HWND radioBtn = CreateWindow(
        TEXT("button"),
        defaultText.c_str(),
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        xCoord, yCoord,  /* x, y coords */
        width, height, /* width, height */
        parent,
        elementId,
        hInstance,
        NULL
    );
    SendMessage(radioBtn, WM_SETFONT, (WPARAM)hFont, true);
    if (checked) {
        SendMessage(radioBtn, BM_SETCHECK, (WPARAM)hFont, true);
    }
}

void doAudioCapture(WASAPICapturerInfo* audioCaptureInfo) {
    CWASAPICapture* capturer = *(audioCaptureInfo->capturer);
    BYTE* captureBuffer = *(audioCaptureInfo->buffer);
    std::cout << "do audio capture start\n";

    PostMessage(audioCaptureInfo->mainWindow, ID_IN_PROGRESS, 0, 0);

    // TODO: allowing only integer duration feels pretty restrictive
    // and might capture more audio/screentime than desired.
    //
    // maybe we should allow users to stop recording arbitrarily
    // and then we can also remove the cap on duration time
    int targetDurationInMs = audioCaptureInfo->durationInMs;

    // maybe this isn't a good idea but try converting ms to s.
    float durationInSec = ceil(targetDurationInMs / 1000.f);
    if (durationInSec < 1) durationInSec = 1;

    int targetDurationInSec = (int)durationInSec;

    do {
        Sleep(1000);
    } while (--targetDurationInSec);

    capturer->Stop();

    //
    //  We've now captured our wave data.  Now write it out in a wave file.
    //
    std::cout << "got the audio data. writing to file now...\n";
    std::cout << "wav filename: " << audioCaptureInfo->outputName << "\n";
    SaveWaveData(captureBuffer, capturer->BytesCaptured(), capturer->MixFormat(), audioCaptureInfo->outputName);

    //
    //  Now shut down the capturer and release it we're done.
    //
    capturer->Shutdown();
    SAFE_RELEASE(capturer);
    delete[] captureBuffer;

    PostMessage(audioCaptureInfo->mainWindow, ID_FINISHED, 0, 0);
}

void doScreenCapture(WindowInfo* args){
    // TODO: this should assemble the video using the captured audio and captured images
    HWND mainWindow = args->mainWindow;
    
    PostMessage(mainWindow, ID_IN_PROGRESS, 0, 0);
    
    int nFrames = (int)floor((args->duration * 1000) / args->timeDelay);
    int tDelay = args->timeDelay;
    
    std::string theDir = args->directory;
    std::wstring theText = args->captionText;
    
    // indicate process started 
    PostMessage(mainWindow, ID_IN_PROGRESS, 0, 0);

    getSnapshots(nFrames, tDelay, x1Pos, y1Pos, (x2Pos-x1Pos), (y2Pos-y1Pos), getBMPImageData, args);
    PostMessage(mainWindow, ID_FINISHED, 0, 0);
}

// do all the things in a separate thread (which wil launch 2 child threads of its own)
void doEverything(){
    int audioDuration = captureParams.duration * 1000; // in ms
    int tDelay = captureParams.timeDelay;
    std::string dirName = captureParams.tempDirectory;

    if (captureParams.screenOnly) {
        WaitForSingleObject(
            CreateThread(NULL, 0, processScreenCaptureThread, &captureParams, 0, 0),
            INFINITE
        );
        if(captureParams.minimizeApp) PostMessage(captureParams.guiWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
    } else {
        // set up for audio collection
        audioCaptureInfo.outputName = std::string(dirName); // name the wav output the same as the temp directory of the snapshots
        audioCaptureInfo.mainWindow = captureParams.mainWindow;
        
        setUpForAudioCollection(
            pEnumerator,
            pDevice,
            pAudioClient,
            pCaptureClient,
            pwfx
        );

        std::cout << "bits per sample: " << pwfx->wBitsPerSample << "\n";
        std::cout << "samples per sec: " << pwfx->nSamplesPerSec << "\n";
        std::cout << "num channels: " << pwfx->nChannels << "\n";
        std::cout << "starting capture...\n";

        CWASAPICapture* capturer = new CWASAPICapture(pDevice, true, eConsole);
        if (capturer == NULL) {
            printf("Unable to allocate capturer\n");
            return;
        }

        int targetLatency = 10;
        if (capturer->Initialize(targetLatency)) {
            int targetDurationInMs = audioDuration;
            size_t captureBufferSize = capturer->SamplesPerSecond() * ceil(targetDurationInMs / 1000) * capturer->FrameSize();
            BYTE* captureBuffer = new BYTE[captureBufferSize];
            std::cout << "buffer size: " << captureBufferSize << '\n';

            if (captureBuffer == NULL) {
                printf("Unable to allocate capture buffer\n");
                return;
            }

            audioCaptureInfo.buffer = &captureBuffer;
            audioCaptureInfo.capturer = &capturer;
            audioCaptureInfo.durationInMs = targetDurationInMs;

            if (capturer->Start(captureBuffer, captureBufferSize)) {
                if (captureParams.audioOnly) {
                    WaitForSingleObject(
                        CreateThread(NULL, 0, processAudioThread, &audioCaptureInfo, 0, 0),
                        INFINITE
                    );
                    if (captureParams.minimizeApp) PostMessage(captureParams.guiWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
                } else {
                    // start frame and audio capture process in child threads
                    HANDLE getFramesThread = CreateThread(NULL, 0, processScreenCaptureThread, &captureParams, 0, 0);
                    HANDLE getAudioThread = CreateThread(NULL, 0, processAudioThread, &audioCaptureInfo, 0, 0);
                    HANDLE waitArray[2] = { getFramesThread, getAudioThread };

                    DWORD waitResult = WaitForMultipleObjects(2, waitArray, TRUE, INFINITE);
                    if (waitResult >= WAIT_OBJECT_0 + 0 && waitResult < WAIT_OBJECT_0 + 2) {
                        // all child threads have completed
                        if (captureParams.minimizeApp) PostMessage(captureParams.guiWindow, WM_SYSCOMMAND, SC_RESTORE, 0);

                        if (!captureParams.ffmpegExists) {
                            // TODO: post msg that audio + images collected + no ffmpeg so finished?
                            std::cout << "done capturing images and audio!\n";

                            if (captureParams.cleanupFiles) {
                                int numFrames = (int)floor((captureParams.duration * 1000) / captureParams.timeDelay);

                                for (int i = 0; i < numFrames; i++) {
                                    // delete each file first
                                    DeleteFileA((dirName + "/screen" + std::to_string(i) + ".bmp").c_str());
                                }
                                // delete the dir
                                RemoveDirectoryA(dirName.c_str());

                                // delete the wav file
                                DeleteFileA((dirName + ".wav").c_str());

                                std::cout << "done cleaning up!\n";
                            }

                            return;
                        }

                        // assemble the video file using the captured screenshots and audio
                        std::cout << "data collection done. creating the video file for: " << dirName << "...\n";

                        float framerate = 1000.0f / tDelay; // frames per sec

                        // example: ffmpeg -framerate 8.3 -i ./temp_14-08-2022_183147/screen%d.bmp -i temp_14-08-2022_183147.wav -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" -c:v libx264 -pix_fmt yuv420p -r 8 testing.mp4
                        std::string command(
                            std::string("ffmpeg ") +
                            std::string(" -framerate ") +
                            std::to_string(framerate).c_str() +
                            std::string(" -i ./") +
                            dirName +
                            std::string("/screen%d.bmp") +
                            std::string(" -i ") +
                            dirName +
                            std::string(".wav") +
                            std::string(" -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\"") +
                            std::string(" -c:v libx264 -pix_fmt yuv420p ") +
                            std::string(" -r ") +
                            std::to_string(framerate).c_str() +
                            std::string(" ") + // can try -shortest flag if needed
                            dirName +
                            std::string(".mp4")
                        );

                        std::cout << "attempting to run: " << command << "\n";

                        // TODO: need to test when ffmpeg not available. I think if ffmpeg is not available there still would be a non-zero value returned.
                        int res = system(command.c_str());
                        if (res != 0) {
                            // TODO: post a message on the UI about failure
                            std::cout << "processing failed :( do you have ffmpeg? \n";
                        }
                        else {
                            std::cout << "processing complete :)\n";

                            if (captureParams.cleanupFiles) {
                                int numFrames = (int)floor((captureParams.duration * 1000) / captureParams.timeDelay);

                                for (int i = 0; i < numFrames; i++) {
                                    // delete each file first
                                    DeleteFileA((dirName + "/screen" + std::to_string(i) + ".bmp").c_str());
                                }
                                // delete the dir
                                RemoveDirectoryA(dirName.c_str());

                                // delete the wav file
                                DeleteFileA((dirName + ".wav").c_str());

                                std::cout << "done cleaning up!\n";
                            }
                        }
                    }
                }
            }
        }
    }
}


/***

    this function, which does the screen capture, is executed by a new thread.
    pass it a pointer to a struct that contains the customizable options

***/
DWORD WINAPI processScreenCaptureThread(LPVOID windowInfo){
    doScreenCapture((WindowInfo*)windowInfo);
    return 0;
}

/***

    this function, which does the audio capturing, is executed by a new thread.
    pass it a pointer to a struct that contains parameters needed for collecting the audio

***/
DWORD WINAPI processAudioThread(LPVOID audioCaptureInfo){
    doAudioCapture((WASAPICapturerInfo*)audioCaptureInfo);
    return 0;
}

/***

    this function does all the things in a new thread (and spawns 2 child threads for audio and screen capture).

***/
DWORD WINAPI doEverythingThread(LPVOID args){
    doEverything();
    return 0;
}



/***

    the window procedure for the GUI (i.e. switching between main page, parameters page, closing the window)
    
***/
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_CREATE:
        {
            // create the menu tabs (main page, parameter page tabs)
            HMENU hMenu;
            hMenu = CreateMenu();
            AppendMenu(hMenu, MF_STRING, ID_MAIN_PAGE, L"Main");
            AppendMenu(hMenu, MF_STRING, ID_SET_PARAMETERS_PAGE, L"Options");
            AppendMenu(hMenu, MF_STRING, ID_SET_ABOUT_PAGE, L"About");
            SetMenu(hwnd, hMenu);

            captureParams.guiWindow = hwnd;
        }
        break;
        
        case WM_COMMAND:
        {
            /* LOWORD takes the lower 16 bits of wParam => the element clicked on */
            switch(LOWORD(wParam)){
                case ID_MAIN_PAGE:
                {
                    // go back to main page 
                    ShowWindow(aboutPage, SW_HIDE);
                    ShowWindow(parameterPage, SW_HIDE);
                    ShowWindow(mainPage, SW_SHOW);
                    UpdateWindow(hwnd);
                }
                break;
                
                case ID_SET_PARAMETERS_PAGE:
                {
                    // switch to parameters page 
                    ShowWindow(mainPage, SW_HIDE);
                    ShowWindow(aboutPage, SW_HIDE);
                    ShowWindow(parameterPage, SW_SHOW);
                    UpdateWindow(hwnd);
                }
                break;
                
                case ID_SET_ABOUT_PAGE:
                {
                    ShowWindow(mainPage, SW_HIDE);
                    ShowWindow(parameterPage, SW_HIDE);
                    ShowWindow(aboutPage, SW_SHOW);
                    UpdateWindow(hwnd);
                }
                break;
                
                case WM_CLOSE:
                {
                    DeleteObject(hFont);
                    DestroyWindow(hwnd);
                }
                break;
                
                case WM_DESTROY:
                {
                    DeleteObject(hFont);
                    PostQuitMessage(0);
                }
                break;
            }        
        }
        break; 
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/***

    procedure for the main page (i.e. select area, start)

***/
LRESULT CALLBACK WndProcMainPage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_COMMAND:
        {
            // LOWORD takes the lower 16 bits of wParam => the element clicked on
            switch(LOWORD(wParam)){
                case ID_SELECT_SCREENAREA_BUTTON:
                {
                    // make a new window to select the area 
                    int x = GetSystemMetrics(SM_CXSCREEN);
                    int y = GetSystemMetrics(SM_CYSCREEN);
                    selectionWindow = CreateWindowEx(
                        WS_EX_LAYERED,
                        g_szClassName4,
                        L"selection",
                        WS_TILEDWINDOW,
                        0, 0, x, y,
                        NULL, NULL, GetModuleHandle(NULL), NULL
                    );

                    // make window transparent
                    SetLayeredWindowAttributes(
                        selectionWindow,
                        0, 
                        (255 * 70) / 100, 
                        LWA_ALPHA
                    );

                    // remove title bar so the top of screen can be selected
                    SetWindowLongPtr(selectionWindow, GWL_STYLE, 0);
                    
                    // show window
                    ShowWindow(selectionWindow, SW_MAXIMIZE);
                    UpdateWindow(selectionWindow);
                }
                break;
                
                case ID_START_BUTTON:
                {
                    // minimize app window
                    if (captureParams.minimizeApp) PostMessage(captureParams.guiWindow, WM_SYSCOMMAND, SC_MINIMIZE, 0);

                    // get the parameters 
                    HWND duration = GetDlgItem(hwnd, ID_DURATION_TEXTBOX);
                    HWND delay = GetDlgItem(hwnd, ID_DELAY_TEXTBOX);
                    
                    wchar_t durationAmount[3];
                    wchar_t timeDelay[5];
                    
                    // because numFrames and timeDelay are initialized as arrays, we can use sizeof to get the number of bytes they occupy
                    GetWindowText(duration, durationAmount, sizeof(durationAmount));
                    GetWindowText(delay, timeDelay, sizeof(timeDelay));
                    
                    int dur = _wtoi(durationAmount);
                    int tDelay = _wtoi(timeDelay);
                    
                    // validate values!!
                    // TODO: #define max/min values?
                    if(dur < 1){
                        dur = 1;
                    }else if(dur > 30){
                        dur = 30;
                    }
                    
                    if(tDelay < 10){
                        tDelay = 10;
                    }else if(tDelay > 1000){
                        tDelay = 1000;
                    }
                    
                    // also, get the currently selected filter
                    HWND filterbox = GetDlgItem(hwnd, ID_FILTERS_COMBOBOX);
                    int currFilterIndex = SendMessage(filterbox, CB_GETCURSEL, 0, 0);
                    
                    // check if user wants a caption! if there's text entered in the textbox for ID_CAPTION_MSG
                    HWND captionText = GetDlgItem(hwnd, ID_CAPTION_MSG);
                    int textLen = GetWindowTextLength(captionText);
                    int captionSize = textLen + 1; // +1 for null term

                    // some compilers like g++ are ok with dynamic array sizes: TCHAR captext[captionSize];
                    // but not MSVC
                    TCHAR* captext = new TCHAR[captionSize];
                    GetWindowText(captionText, captext, captionSize);

                    std::cout << "creating temp dir for snapshots...\n";

                    // get curr time to use for naming the output 
                    std::time_t timeNow = std::time(NULL);
                    std::tm ptm;
                    localtime_s(&ptm, &timeNow);

                    char buff[32];
                    std::strftime(buff, 32, "%d-%m-%Y_%H%M%S", &ptm);
                    std::string currTime = std::string(buff);

                    // make a temp directory 
                    std::string dirName = "temp_" + std::string(currTime.begin(), currTime.end());
                    captureParams.tempDirectory = dirName;

                    if (captureParams.audioAndScreen || captureParams.screenOnly) {
                        if (CreateDirectoryA(dirName.c_str(), NULL)) {
                            // do nothing
                        }
                        else if (ERROR_ALREADY_EXISTS == GetLastError()) {
                            // if it exists, empty out the directory
                        }
                        else {
                            // directory couldn't be made
                        }
                    }

                    captureParams.duration = dur;
                    captureParams.timeDelay = tDelay;
                    captureParams.selectedFilter = currFilterIndex;
                    captureParams.directory = std::string("");
                    captureParams.captionText = std::wstring(captext);
                    captureParams.mainWindow = hwnd;

                    delete[] captext;

                    CreateThread(NULL, 0, doEverythingThread, NULL, 0, 0);
                }
                break;
            }
        }
        break;
        
        case ID_IN_PROGRESS:
        {
            SetDlgItemText(hwnd, ID_PROGRESS_MSG, L"processing...");
        }
        break;
        
        case ID_FINISHED:
        {
            SetDlgItemText(hwnd, ID_PROGRESS_MSG, L"processing successful!");
        }
        break;
        
        case ID_UNABLE_TO_OPEN:
        {
            SetDlgItemText(hwnd, ID_PROGRESS_MSG, L"unable to open directory");
        }
        break;
        
        case ID_NO_BMPS_FOUND:
        {
            SetDlgItemText(hwnd, ID_PROGRESS_MSG, L"no bmp images were found");
        }
        break;
        
        case ID_COLLECTING_IMAGES:
        {
            SetDlgItemText(hwnd, ID_PROGRESS_MSG, L"capturing...");
        }
        break;
        
        case ID_PROCESS_FRAME:
        {
            // for this particular message we want to know which frame is being processed, 
            // so we can use WPARAM as an int 
            int currFrame = (int)wParam;
            //std::cout << "curr frame: " << currFrame << std::endl;
            std::wstring msg = L"processing frame: " + std::to_wstring(currFrame);
            SetDlgItemText(hwnd, ID_PROGRESS_MSG, msg.c_str());
        }
        break;
        
        case WM_CLOSE:
        {
            DeleteObject(hFont);
            DestroyWindow(hwnd);
        }
        break;
        
        case WM_DESTROY:
        {
            DeleteObject(hFont);
            PostQuitMessage(0);
        }
        break;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// return the selected color for the selection screen 
COLORREF getSelectedColor(HWND selectBox){
    int currColorIndex = SendMessage(selectBox, CB_GETCURSEL, 0, 0);
    //std::cout << "currColorIndex: " << currColorIndex << std::endl;
    if(currColorIndex == 1){
        return (COLORREF)RGB(140,180,255);
    }else if(currColorIndex == 2){
        return (COLORREF)RGB(140,255,180);
    }
    
    return COLOR;
}

LRESULT CALLBACK WndProcParameterPage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_LBUTTONDOWN:
        {
            case WM_COMMAND:
            {
                /* LOWORD takes the lower 16 bits of wParam => the element clicked on */
                switch(LOWORD(wParam)){
                    // there should be a save button option
                    case ID_SAVE_PARAMETERS:
                    {
                        // get the selected color for the screen! 
                        HWND colorSelect = GetDlgItem(hwnd, ID_SELECTION_COLOR);
                        captureParams.selectionWindowColor = getSelectedColor(colorSelect);
                        
                        // get the saturation value 
                        HWND saturation = GetDlgItem(hwnd, ID_SET_SATURATION);
                        CHAR saturationValue[5];
                        GetWindowTextA(saturation, saturationValue, sizeof(saturationValue));
                        float satVal = strtof(saturationValue, NULL);
                        captureParams.saturationValue = satVal > 10.0f ? 10.0f : satVal;
                        SetDlgItemTextA(hwnd, ID_SET_SATURATION, floatToString(captureParams.saturationValue).c_str());
                        
                        // get the mosaic chunk size value
                        HWND mosaic = GetDlgItem(hwnd, ID_SET_MOSAIC);
                        TCHAR mosaicValue[5];
                        GetWindowText(mosaic, mosaicValue, sizeof(mosaicValue));
                        int mosVal = _wtoi(mosaicValue);
                        captureParams.mosaicChunkSize = mosVal > 80 ? 80 : mosVal;
                        SetDlgItemText(hwnd, ID_SET_MOSAIC, std::to_wstring(captureParams.mosaicChunkSize).c_str());
                        
                        // get the outline size value 
                        HWND outline = GetDlgItem(hwnd, ID_SET_OUTLINE);
                        TCHAR outlineValue[5];
                        GetWindowText(outline, outlineValue, sizeof(outlineValue));
                        int outlineVal = _wtoi(outlineValue);
                        captureParams.outlineColorDiffLimit = outlineVal > 20 ? 20 : outlineVal;
                        SetDlgItemText(hwnd, ID_SET_OUTLINE, std::to_wstring(captureParams.outlineColorDiffLimit).c_str());
                        
                        // get the Voronoi neighbor constant value
                        HWND voronoi = GetDlgItem(hwnd, ID_SET_VORONOI);
                        TCHAR voronoiValue[5];
                        GetWindowText(voronoi, voronoiValue, sizeof(voronoiValue));
                        int voronoiConst = _wtoi(voronoiValue);
                        captureParams.voronoiNeighborConstant = voronoiConst > 60 ? 60 : voronoiConst;
                        SetDlgItemText(hwnd, ID_SET_VORONOI, std::to_wstring(captureParams.voronoiNeighborConstant).c_str());
                        
                        // get the blur factor value
                        HWND blur = GetDlgItem(hwnd, ID_SET_BLUR);
                        TCHAR blurValue[5];
                        GetWindowText(blur, blurValue, sizeof(blurValue));
                        int blurVal = _wtoi(blurValue);
                        captureParams.blurFactor = blurVal > 10 ? 10 : blurVal;
                        SetDlgItemText(hwnd, ID_SET_BLUR, std::to_wstring(captureParams.blurFactor).c_str());
                        
                        // get the value of 'show cursor' checkbox 
                        HWND getCursorBox = GetDlgItem(hwnd, ID_GET_CURSOR);
                        int getCursorVal = SendMessage(getCursorBox, BM_GETCHECK, 0, 0);
                        captureParams.getCursor = (getCursorVal == BST_CHECKED);

                        HWND getCleanupCursorBox = GetDlgItem(hwnd, ID_CLEANUP_FILES);
                        int getCleanupCursorVal = SendMessage(getCleanupCursorBox, BM_GETCHECK, 0, 0);
                        captureParams.cleanupFiles = (getCleanupCursorVal == BST_CHECKED);

                        HWND getMinimizeApp = GetDlgItem(hwnd, ID_MINIMIZE_APP);
                        int getMinimizeAppVal = SendMessage(getMinimizeApp, BM_GETCHECK, 0, 0);
                        captureParams.minimizeApp = (getMinimizeAppVal == BST_CHECKED);

                        // specify if capture should be audio only, screen only or both
                        HWND getAudioOnly = GetDlgItem(hwnd, ID_AUDIO_ONLY);
                        int getAudioOnlyVal = SendMessage(getAudioOnly, BM_GETCHECK, 0, 0);
                        captureParams.audioOnly = (getAudioOnlyVal == BST_CHECKED);

                        HWND getScreenOnly = GetDlgItem(hwnd, ID_SCREEN_ONLY);
                        int getScreenOnlyVal = SendMessage(getScreenOnly, BM_GETCHECK, 0, 0);
                        captureParams.screenOnly = (getScreenOnlyVal == BST_CHECKED);

                        HWND getAudioAndScreen = GetDlgItem(hwnd, ID_AUDIO_AND_SCREEN);
                        int getAudioAndScreenVal = SendMessage(getAudioAndScreen, BM_GETCHECK, 0, 0);
                        captureParams.audioAndScreen = (getAudioAndScreenVal == BST_CHECKED);
                    }
                }
            }
            break;
        }
        break;
        
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
        
        case WM_DESTROY:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// reset helper function 
void reset(POINT *p1, POINT *p2, bool *drag, bool *draw){
    *drag = false;
    *draw = false;
    p1->x = 0;
    p1->y = 0;
    p2->x = 0;
    p2->y = 0;
}

LRESULT CALLBACK WndProcSelection(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_LBUTTONDOWN:
        {
            SetCapture(hwnd);
            
            // need button down to initiate mousemove step 
            bDrag = true;

            // log the coordinates 
            GetCursorPos(&ptCurr);
            
            // check out the difference between ScreenToClient vs ClientToScreen 
            ScreenToClient(hwnd, &ptCurr);
        }
        break;
        
        case WM_MOUSEMOVE:
        {
            if(bDrag){
                bDraw = true;
                
                // log the coordinates 
                GetCursorPos(&ptNew);
                
                // convert coordinates
                ScreenToClient(hwnd, &ptNew);
                bDrag = false;
            }else if(bDraw){  
                HDC hdc = GetDC(hwnd);
                SelectObject(hdc,GetStockObject(DC_BRUSH));
                SetDCBrushColor(hdc, captureParams.selectionWindowColor);
                
                SetROP2(hdc, R2_NOTXORPEN);
                
                // erase old rectangle 
                Rectangle(hdc, ptCurr.x, ptCurr.y, ptNew.x, ptNew.y);
                
                // collect current coordinates, set them as new 
                GetCursorPos(&ptNew);
                ScreenToClient(hwnd, &ptNew);
                
                // draw new rectangle 
                Rectangle(hdc, ptCurr.x, ptCurr.y, ptNew.x, ptNew.y);
                ReleaseDC(hwnd, hdc);
            }
        }
        break;
        
        case WM_LBUTTONUP:
        {
            if(bDraw){
                // check if ok with user 
                int response = MessageBox(hwnd, L"Is this selection okay?", L"Confirm Selection", MB_YESNOCANCEL);
                if(response == IDCANCEL){
                    reset(&ptCurr, &ptNew, &bDrag, &bDraw);
                    ReleaseCapture();
                    DestroyWindow(hwnd);
                    return 0;             
                }else if(response == IDYES){
                    // done, record new parameters 
                    // note that this can be a bit tricky because normally I'd expect to draw the selection rectangle
                    // from top left to bottom right. however, you should allow for rectangles to be drawn starting from 
                    // wherever the user wants. if you just assume one way (the expected way), you can get negative widths and heights!
                    // therefore, for x1 and x2, x1 should be the min of the 2, and x2 the max. likewise for y1 and y2.
                    x1Pos = min(ptCurr.x, ptNew.x);
                    y1Pos = min(ptCurr.y, ptNew.y);
                    x2Pos = max(ptCurr.x, ptNew.x);
                    y2Pos = max(ptCurr.y, ptNew.y);
                    
                    // do some correction to take into account the title bar height 
                    if(y1Pos > 0) y1Pos = y1Pos + GetSystemMetrics(SM_CYCAPTION);
                    
                    // also adjust y2, the endpoint for the y-coord
                    y2Pos = y2Pos + GetSystemMetrics(SM_CYCAPTION);
                    
                    reset(&ptCurr, &ptNew, &bDrag, &bDraw);
                    DestroyWindow(hwnd);
                    ReleaseCapture();
                    return 0;      
                }else if(response == IDNO){
                    // need to clear screen!!
                    HDC hdc = GetDC(hwnd);
                    SelectObject(hdc,GetStockObject(DC_BRUSH));
                    SetDCBrushColor(hdc, captureParams.selectionWindowColor); 
                    SetROP2(hdc, R2_NOTXORPEN);
                    // erase old rectangle 
                    Rectangle(hdc, ptCurr.x, ptCurr.y, ptNew.x, ptNew.y);
                    ReleaseDC(hwnd, hdc);
                    // reset stuff
                    reset(&ptCurr, &ptNew, &bDrag, &bDraw);
                }else{
                    // failure to show message box    
                    std::cout << "error with message box!!" << std::endl;
                    ReleaseCapture();
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
        }
        break;
        
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
        
        case WM_DESTROY:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


LRESULT CALLBACK WndProcAboutPage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
        
        case WM_DESTROY:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


/***
    this function creates the UI for the main page (the first screen you see)
    it takes a window handler (HWND) as an argument that the UI will be drawn on 
    and an HINSTANCE
***/
void createMainScreen(HWND hwnd, HINSTANCE hInstance){
    // set duration to record in seconds
    // TODO: does there really need to be a limit? would need to test first (the num of frames captured could blow up and not sure if that might break things?)
    createLabel(L"duration (sec): ", 140, 20, 10, 20, hwnd, hInstance, (HMENU)ID_DURATION_LABEL, hFont);
    createEditBox(L"5", 80, 20, 110, 20, hwnd, hInstance, (HMENU)ID_DURATION_TEXTBOX, hFont);
    createLabel(L"1 <= duration <= 30", 130, 20, 210, 20, hwnd, hInstance, NULL, hFont);
    
    // set interval between frames
    createLabel(L"interval (ms): ", 100, 20, 10, 50, hwnd, hInstance, (HMENU)ID_DELAY_LABEL, hFont);
    createEditBox(L"120", 80, 20, 110, 50, hwnd, hInstance, (HMENU)ID_DELAY_TEXTBOX, hFont);
    createLabel(L"10 <= ms <= 1000", 130, 20, 210, 50, hwnd, hInstance, NULL, hFont);
    
    // select image filter label
    createLabel(L"filter options: ", 80, 20, 10, 85, hwnd, hInstance, (HMENU)ID_FILTERS_LABEL, hFont);
    
    // combobox to select image filter
    HWND filterComboBox = CreateWindow(
        WC_COMBOBOX,
        TEXT(""),
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE, 
        110, 80, 
        80, 250,
        hwnd,
        (HMENU)ID_FILTERS_COMBOBOX,
        hInstance,
        NULL
    );
    SendMessage(filterComboBox, WM_SETFONT, (WPARAM)hFont, true);
    
    // add filter options to dropdown
    std::map<int, std::wstring>::iterator it = filterMap.begin();
    while(it != filterMap.end()){
        SendMessage(filterComboBox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)(it->second).c_str());
        it++;
    }
    
    // initially the filter is set to "none"
    SendMessage(filterComboBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
    
    /* 
        let user add a caption to the result. for now, it'll be a bit limited in that the text 
        will automatically be placed towards the bottom. 
        it will however, be centered (and so some calculations are needed)
        the amount of text will vary depending on image size as well
        font will also be Impact and size will be determined by program 
    */
    createLabel(
        L"specify a message to show at bottom of video: ",
        340, 20,
        10, 120,
        hwnd,
        hInstance,
        NULL,
        hFont
    );
    
    createEditBox(L"", 280, 20, 10, 150, hwnd, hInstance, (HMENU)ID_CAPTION_MSG, hFont);
    
    // button to select area of screen
    HWND selectAreaButton = CreateWindow(
        TEXT("button"),
        TEXT("select area"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 260,
        80, 20,
        hwnd,
        (HMENU)ID_SELECT_SCREENAREA_BUTTON,
        hInstance,
        NULL
    );
    SendMessage(selectAreaButton, WM_SETFONT, (WPARAM)hFont, true);
    
    // button to start the screen capture
    HWND startButton = CreateWindow(
        TEXT("button"),
        TEXT("start"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 290,
        80, 20,
        hwnd,
        (HMENU)ID_START_BUTTON,
        hInstance,
        NULL
    );
    SendMessage(startButton, WM_SETFONT, (WPARAM)hFont, true);
    
    // text indicator/message for processing progress
    HWND progressBar = CreateWindow(
        TEXT("STATIC"),
        TEXT(""),
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        80, 330,  /* x, y coords */
        220, 20, /* width, height */
        hwnd,
        (HMENU)ID_PROGRESS_MSG,
        hInstance,
        NULL
    );
    SendMessage(progressBar, WM_SETFONT, (WPARAM)hFont, true);
}

/***
    this function sets up the parameters page, where the user can change certain parameters
    like for image filters, or to change the color of the selection screen 
***/
void createParameterPage(HWND hwnd, HINSTANCE hInstance){
    createLabel(L"choose selection screen color: ", 180, 20, 10, 12, hwnd, hInstance, NULL, hFont);
        
    HWND setColorBox = CreateWindowW(
        WC_COMBOBOX,
        TEXT(""),
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE, 
        210, 10,  /* x, y coords */
        85, 80, /* width, height */
        hwnd,
        (HMENU)ID_SELECTION_COLOR,
        hInstance,
        NULL
    );
    SendMessage(setColorBox, WM_SETFONT, (WPARAM)hFont, true);
    SendMessage(setColorBox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)L"light red");
    SendMessage(setColorBox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)L"light blue");
    SendMessage(setColorBox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)L"light green");
    SendMessage(setColorBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
    
    // set saturation value (float) for saturation filter
    createLabel(L"set saturation value: ", 200, 20, 10, 50, hwnd, hInstance, NULL, hFont);
    createEditBox(L"2.1", 50, 20, 210, 50, hwnd, hInstance, (HMENU)ID_SET_SATURATION, hFont);
    
    // set mosaic chunk size for the mosaic filter
    createLabel(L"set mosaic chunk size: ", 200, 20, 10, 80, hwnd, hInstance, NULL, hFont);
    createEditBox(L"10", 50, 20, 210, 78, hwnd, hInstance, (HMENU)ID_SET_MOSAIC, hFont);
    
    // set the difference limit allowed betweeen 2 pixel colors (int) for outline filter
    createLabel(L"set outline difference limit: ", 200, 20, 10, 110, hwnd, hInstance, NULL, hFont);
    createEditBox(L"10", 50, 20, 210, 110, hwnd, hInstance, (HMENU)ID_SET_OUTLINE, hFont);
    
    // set neighbor constant for Voronoi
    createLabel(L"set Voronoi neighbor constant: ", 180, 20, 10, 140, hwnd, hInstance, NULL, hFont);
    createEditBox(L"30", 50, 20, 210, 140, hwnd, hInstance, (HMENU)ID_SET_VORONOI, hFont);
    
    // set blur factor
    createLabel(L"set blur factor: ", 170, 20, 10, 170, hwnd, hInstance, NULL, hFont);
    createEditBox(L"3", 50, 20, 210, 170, hwnd, hInstance, (HMENU)ID_SET_BLUR, hFont);

    // set whether to capture the cursor or not
    createCheckBox(L"capture screen cursor", 180, 20, 10, 205, hwnd, hInstance, (HMENU)ID_GET_CURSOR, hFont);

    // whether the captured frames (.bmps) and audio (.wav) should be deleted after video creation
    createCheckBox(L"cleanup files after video creation", 250, 20, 10, 225, hwnd, hInstance, (HMENU)ID_CLEANUP_FILES, hFont);

    // whether to minimize the app when recording
    createCheckBox(L"minimize when recording", 250, 20, 10, 245, hwnd, hInstance, (HMENU)ID_MINIMIZE_APP, hFont);

    // create radio buttons to choose whether to record just audio, just the screen or both
    createRadioButton(L"capture audio only", 150, 20, 10, 275, hwnd, hInstance, (HMENU)ID_AUDIO_ONLY, hFont);
    createRadioButton(L"capture screen only", 150, 20, 10, 295, hwnd, hInstance, (HMENU)ID_SCREEN_ONLY, hFont);
    createRadioButton(L"capture audio and screen", 170, 20, 10, 315, hwnd, hInstance, (HMENU)ID_AUDIO_AND_SCREEN, hFont, true);
    
    HWND saveParameters = CreateWindow(
        TEXT("button"),
        TEXT("save"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 352,
        80, 20, 
        hwnd,
        (HMENU)ID_SAVE_PARAMETERS,
        hInstance,
        NULL
    );
    SendMessage(saveParameters, WM_SETFONT, (WPARAM)hFont, true);
}


void createAboutPage(HWND hwnd, HINSTANCE hInstance){
    HWND title;
    title = CreateWindow(
        TEXT("STATIC"),
        TEXT(" \n    An application for screen capturing with audio.\n    Currently requires ffmpeg.\n\n\n    (c) nch 2022 | https://github.com/syncopika\n\n"),
        WS_VISIBLE | WS_CHILD,
        0, 0,
        400, 450,
        hwnd, /* parent window */
        NULL,
        hInstance,
        NULL
    );
    // send the gui the font to use 
    SendMessage(title, WM_SETFONT, (WPARAM)hFont, true);
}

/*************

    MAIN METHOD FOR GUI
    
**************/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
    /* console attached for debugging std::cout statements */
    FILE* stream;
    AllocConsole();
    freopen_s(&stream, "CON", "w", stdout);

    char psBuffer[128];
    FILE* pPipe;
    bool ffmpegExists = false;

    // check for existence of ffmpeg via where command
    if((pPipe = _popen("where ffmpeg", "rt")) == NULL){
        exit(1);
    }
    
    fgets(psBuffer, 128, pPipe);
    //printf(psBuffer);

    // if "where ffmpeg" can't find ffmpeg, I get the following output in the console:
    // 'INFO: Could not find files for the given pattern(s).'
    // not seeing that output in the buffer though if I try something like "where asdf"
    // so guessing that the buffer might be empty and INFO is coming from somewhere else?
    // _pclose seems to return 1 when where fails to find a path (e.g. with "where asdf")
    // and 0 when a path is found so maybe I can use that

    int pclose = _pclose(pPipe);
    //printf("process returned %d\n", pclose);

    if(pclose == 0){
        ffmpegExists = true;
    }else{
        std::cout << "ffmpeg not found on PATH!\n";
    }
    
    // add some default parameters to captureParams immediately
    captureParams.filters = filterMap;
    captureParams.selectionWindowColor = COLOR;
    captureParams.saturationValue = 2.1f;
    captureParams.mosaicChunkSize = 30;
    captureParams.outlineColorDiffLimit = 10;
    captureParams.voronoiNeighborConstant = 30;
    captureParams.blurFactor = 3;
    captureParams.getCursor = false;
    captureParams.cleanupFiles = false;
    captureParams.ffmpegExists = ffmpegExists;
    captureParams.audioOnly = false;
    captureParams.screenOnly = false;
    captureParams.audioAndScreen = true;
    captureParams.minimizeApp = false;
    
    // for improving the gui appearance (buttons, that is. the font needs to be changed separately) 
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    
    MSG Msg;
    
    // make a main window
    WNDCLASSEX wc; // this is the main GUI window
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszMenuName = NULL; 
    wc.lpszClassName = g_szClassName;
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
    wc.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
    
    // register the main screen (the contents i.e. text boxes, labels, etc.), which is a child of the main window
    WNDCLASSEX wc1;
    wc1.cbSize = sizeof(WNDCLASSEX);
    wc1.style = 0;
    wc1.lpfnWndProc = WndProcMainPage;
    wc1.cbClsExtra = 0;
    wc1.cbWndExtra = 0;
    wc1.hInstance = hInstance;
    wc1.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc1.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc1.lpszMenuName = NULL; 
    wc1.lpszClassName = g_szClassName2;
    wc1.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc1.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    // register the second window class - this is the parameters/options page
    WNDCLASSEX wc2;
    wc2.cbSize = sizeof(WNDCLASSEX);
    wc2.style = 0;
    wc2.lpfnWndProc = WndProcParameterPage;
    wc2.cbClsExtra = 0;
    wc2.cbWndExtra = 0;
    wc2.hInstance = hInstance;
    wc2.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc2.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc2.lpszMenuName = NULL;
    wc2.lpszClassName = g_szClassName3;
    wc2.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc2.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    // register the third window class - this is the selection window
    WNDCLASSEX wc3;
    wc3.cbSize = sizeof(WNDCLASSEX);
    wc3.style = 0;
    wc3.lpfnWndProc = WndProcSelection;
    wc3.cbClsExtra = 0;
    wc3.cbWndExtra = 0;
    wc3.hInstance = hInstance;
    wc3.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc3.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc3.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc3.lpszMenuName = NULL;
    wc3.lpszClassName = g_szClassName4;
    wc3.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    // this is the about page window class
    WNDCLASSEX wc4;
    wc4.cbSize = sizeof(WNDCLASSEX);
    wc4.style = 0;
    wc4.lpfnWndProc = WndProcAboutPage; 
    wc4.cbClsExtra = 0;
    wc4.cbWndExtra = 0;
    wc4.hInstance = hInstance;
    wc4.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc4.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc4.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc4.lpszMenuName = NULL;
    wc4.lpszClassName = g_szClassName5;
    wc4.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    
    if(!RegisterClassEx(&wc)){
        std::cout << "error code: " << GetLastError() << std::endl;
        MessageBox(NULL, L"window registration failed for the main GUI!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(!RegisterClassEx(&wc1)){
        std::cout << "error code: " << GetLastError() << std::endl;
        MessageBox(NULL, L"window registration failed for main page!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(!RegisterClassEx(&wc2)){
        std::cout << "error code: " << GetLastError() << std::endl;
        MessageBox(NULL, L"window registration failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(!RegisterClassEx(&wc3)){
        std::cout << "error code: " << GetLastError() << std::endl;
        MessageBox(NULL, L"window registration failed for selection screen!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(!RegisterClassEx(&wc4)){
        std::cout << "error code: " << GetLastError() << std::endl;
        MessageBox(NULL, L"window registration failed for selection screen!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // create the window
    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        L"basic_screen_recorder",
        (WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX) & ~WS_MAXIMIZEBOX, // this combo disables maximizing and resizing the window
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 450,
        NULL, NULL, hInstance, NULL
    );
    
    // create the main screen
    mainPage = CreateWindowEx(
        WS_EX_WINDOWEDGE, // border with raised edge 
        g_szClassName2,
        NULL,
        WS_CHILD,
        0, 0, 400, 450,
        hwnd, // parent window 
        NULL,
        hInstance, NULL
    );
    
    // create the parameters page
    parameterPage = CreateWindowEx(
        WS_EX_WINDOWEDGE,
        g_szClassName3,
        NULL,
        WS_CHILD,
        0, 0, 400, 450,
        hwnd, // parent window
        NULL,
        hInstance, NULL
    );
    
    aboutPage = CreateWindowEx(
        WS_EX_WINDOWEDGE,
        g_szClassName5,
        NULL,
        WS_CHILD,
        0, 0, 400, 450,
        hwnd, // parent window
        NULL,
        hInstance, NULL
    );
    
    if(hwnd == NULL){
        //std::cout << "error code: " << GetLastError() << std::endl;
        MessageBox(NULL, L"window creation failed for the main GUI!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(mainPage == NULL){
        MessageBox(NULL, L"window creation failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(parameterPage == NULL){
        MessageBox(NULL, L"window creation failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    if(aboutPage == NULL){
        MessageBox(NULL, L"window creation failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // make and show main GUI window
    ShowWindow(hwnd, nCmdShow); // show the GUI 
    UpdateWindow(hwnd);
    
    // show the main screen on the GUI
    createMainScreen(mainPage, hInstance); // create the main screen
    ShowWindow(mainPage, SW_SHOW);
    UpdateWindow(hwnd);
    
    // create the parameter and about pages (but don't show yet)
    createParameterPage(parameterPage, hInstance);
    createAboutPage(aboutPage, hInstance);
    
    /* message loop */
    while(GetMessage(&Msg, NULL, 0, 0) > 0){
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    
    return Msg.wParam;
}