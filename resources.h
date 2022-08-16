// hold the IDs used for the GUI and icon

#define ID_MAIN_PAGE 9000
#define ID_SET_PARAMETERS_PAGE 9001
#define ID_SET_ABOUT_PAGE 9002

// not used anymore
#define ID_TITLE_LABEL 99

#define ID_NUMFRAMES_LABEL 100
#define ID_NUMFRAMES_TEXTBOX 101
#define ID_ADD_NUMFRAMES 102 

#define ID_DELAY_LABEL 103 
#define ID_DELAY_TEXTBOX 104
#define ID_ADD_DELAY 105

#define ID_FILTERS_LABEL 109 
#define ID_FILTERS_COMBOBOX 110

#define ID_CHOOSE_DIR 111
#define ID_CHOOSE_DIR_BUTTON 113

#define ID_SELECT_SCREENAREA_BUTTON 106 
#define ID_START_BUTTON 107

#define ID_CAPTION_MSG 112

// the textbox that will hold the progress msgs
#define ID_PROGRESS_MSG 108

#define IDI_ICON 200


// IDs for the parameters page 
#define ID_SELECTION_COLOR 201
#define ID_SAVE_PARAMETERS 202
#define ID_SET_SATURATION 203
#define ID_SET_MOSAIC 204
#define ID_SET_OUTLINE 205
#define ID_SET_VORONOI 206
#define ID_SET_BLUR 207
#define ID_GET_CURSOR 208
#define ID_CLEANUP_FILES 209


// IDs for signalling certain progress messages
// see application-defined msgs
// https://docs.microsoft.com/en-us/windows/desktop/winmsg/about-messages-and-message-queues#application-defined-messages
#define ID_IN_PROGRESS 		 (WM_APP + 0)
#define ID_FINISHED 		 (WM_APP + 1)
#define ID_UNABLE_TO_OPEN	 (WM_APP + 2)
#define ID_NO_BMPS_FOUND 	 (WM_APP + 3)
#define ID_ASSEMBLING_GIF 	 (WM_APP + 4)
#define ID_COLLECTING_IMAGES (WM_APP + 5)
#define ID_PROCESS_FRAME     (WM_APP + 6)
