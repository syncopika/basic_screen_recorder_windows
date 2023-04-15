# basic screen recorder with audio for Windows    
    
This is a screen recording app for Windows. It uses code from my [gif-capturing app](https://github.com/syncopika/gifCatch_desktop-Windows-) and some [WASAPI sample code](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/multimedia/audio/CaptureSharedTimerDriven) that Microsoft has kindly provided.    
    
It does screen capturing and audio recording based on the duration to capture and the time interval between frames set by the user and spits out an .mp4 video file (along with a folder of captured frames and a wav file of the captured audio).    
    
Features include:    
- various image filters
- ability to add a caption
- ability to capture the mouse cursor
- can specify whether to capture audio only, the screen only, or both
    
**Note**: this project relies on `ffmpeg` to create the video file and expects it to be installed. Other than that, no other external dependencies! I used Visual Studio 2019 Community and Windows 10 to build this project.    
    
![screenshot of gui](notes/gui_screenshot.png)    
![screenshot of gui parameters section](notes/gui_screenshot2.png)    
    
## current limitations:    
- capture duration is capped at 30 seconds currently
- `ffmpeg` is expected to be on PATH
    
<details>
<summary>example output from the app </summary>

<video src="https://user-images.githubusercontent.com/8601582/230380457-52cd23d8-d937-4ea8-98a2-3b88c7042be9.mp4"></video> 
from the anime _Bocchi the Rock!_ (ep 4) with saturation filter applied.
    
<hr />

<video src="https://user-images.githubusercontent.com/8601582/230383000-f060cddf-9d3d-47dd-b0a2-326a598fc495.mp4"></video>
from https://www.youtube.com/watch?v=D8gwnKApqCE
</details>