#include <openvr.h>
#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include "shared/Matrices.h"

//-----------------------------------------------------------------------------
// Purpose: Converts a SteamVR matrix to our local matrix class
//-----------------------------------------------------------------------------
inline Matrix4 convert( const vr::HmdMatrix34_t &matPose )
{
	return Matrix4(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
		);
}

inline Matrix4 convert( const vr::HmdMatrix44_t &mat )
{
	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1], 
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2], 
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}

enum class TextureUpdate {Updated, Error, Same};

#define LogMessage printf
class TrackingOnly
{
public:
	bool InitOpenVR(SDL_Renderer*);
	bool StartCapture();
	void ReleaseVR();
	void StopCapture();
	bool UpdateImage();
	bool pollVRforButton();
	void UpdateHMD();

    vr::IVRSystem					*m_pVRSystem;
    vr::IVRTrackedCamera			*m_pVRTrackedCamera;
    vr::TrackedCameraHandle_t	m_hTrackedCamera;

    uint32_t				m_nCameraFrameWidth;
    uint32_t				m_nCameraFrameHeight;
    uint32_t				m_nCameraFrameBufferSize = 0;
    uint8_t					*m_pCameraFrameBuffer=0;
    vr::CameraVideoStreamFrameHeader_t m_frameHeader;
    std::string m_HMDSerialNumberString;
    uint32_t				m_nLastFrameSequence;

    /// SDL Part

    SDL_Surface * m_surface = 0;
	SDL_Rect m_texr;
	SDL_Texture * m_tex = 0;
	SDL_Renderer * m_renderer =0;

	/// Tracking Part

	vr::TrackedDevicePose_t m_rTrackedDevicePose[ vr::k_unMaxTrackedDeviceCount ];
	Matrix4 m_rmat4DevicePose[ vr::k_unMaxTrackedDeviceCount ];
	int m_iValidPoseCount =0;
	std::string m_strPoseClasses;
	char m_rDevClassChar[ vr::k_unMaxTrackedDeviceCount ];   // for each device, a character representing its class

    Matrix4        m_cam2head;
    Matrix4 	   m_hmdpose;
};

void TrackingOnly::UpdateHMD()
{
	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

	m_iValidPoseCount = 0;
	m_strPoseClasses = "";
	for ( int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice )
	{
		if ( m_rTrackedDevicePose[nDevice].bPoseIsValid )
		{
			m_iValidPoseCount++;
			m_rmat4DevicePose[nDevice] = convert( m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking );
			if (m_rDevClassChar[nDevice]==0)
			{
				switch (m_pVRSystem->GetTrackedDeviceClass(nDevice))
				{
				case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
				case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
				case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
				case vr::TrackedDeviceClass_Other:             m_rDevClassChar[nDevice] = 'O'; break;
				case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
				default:                                       m_rDevClassChar[nDevice] = '?'; break;
				}
			}
			m_strPoseClasses += m_rDevClassChar[nDevice];
		}
	}

	if ( m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
	{
		m_hmdpose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd].invert();
	}
}

bool TrackingOnly::pollVRforButton()
{
	bool r = false;
	// Process SteamVR events
	vr::VREvent_t event;
	while( m_pVRSystem->PollNextEvent( &event, sizeof( event ) ) )
	{
		//ProcessVREvent( event );

	}

	// Process SteamVR controller state
	for( vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++ )
	{
		vr::VRControllerState_t state;
		if( m_pVRSystem->GetControllerState( unDevice, &state ) )
		{
			if(state.ulButtonPressed)
			{
				r = true;
				break;
			}
		}
	}
	return r;
}

bool TrackingOnly::StartCapture()
{
    if(!m_renderer)
    {
        LogMessage("Invalid Renderer");
        return false;
    }

    // Allocate for camera frame buffer requirements
    uint32_t nCameraFrameBufferSize = 0;
    if ( m_pVRTrackedCamera->GetCameraFrameSize( vr::k_unTrackedDeviceIndex_Hmd, vr::VRTrackedCameraFrameType_Undistorted, &m_nCameraFrameWidth, &m_nCameraFrameHeight, &nCameraFrameBufferSize ) != vr::VRTrackedCameraError_None )
    {
        LogMessage(  "GetCameraFrameBounds() Failed!\n" );
        return false;
    }

    // used for saving image NOT for showing
    m_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, m_nCameraFrameWidth, m_nCameraFrameHeight, 32,
                                   0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if(!m_surface)

    {
        LogMessage(  "SDL_CreateRGBSurface FAIL\n" );
        return false;

    }
    LogMessage(  "SDL_CreateRGBSurface %d %d OK\n",m_nCameraFrameWidth,m_nCameraFrameHeight );

    m_tex = SDL_CreateTexture(m_renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STREAMING,m_nCameraFrameWidth,m_nCameraFrameHeight);
    if(!m_tex)
         {
        LogMessage(  "SDL_CreateTexture FAIL\n" );
        return false;

    }
    LogMessage(  "SDL_CreateTexture OK\n" );
        {
		int w,h;
                SDL_QueryTexture(m_tex,0,0,&w,&h);
		SDL_Rect texr; texr.x = 0; texr.y = 0; texr.w = w; texr.h = h; 
		m_texr = texr;
	}

    LogMessage(  "SDL_QueryTexture OK\n" );


    if ( nCameraFrameBufferSize && nCameraFrameBufferSize != m_nCameraFrameBufferSize )
    {
        delete [] m_pCameraFrameBuffer;
        m_nCameraFrameBufferSize = nCameraFrameBufferSize;
        m_pCameraFrameBuffer = new uint8_t[m_nCameraFrameBufferSize];
        memset( m_pCameraFrameBuffer, 0, m_nCameraFrameBufferSize );
    }
    LogMessage("Buffer Inited\n");

    m_nLastFrameSequence = 0;

    m_pVRTrackedCamera->AcquireVideoStreamingService( vr::k_unTrackedDeviceIndex_Hmd, &m_hTrackedCamera );
    if ( m_hTrackedCamera == INVALID_TRACKED_CAMERA_HANDLE )
    {
        LogMessage(  "AcquireVideoStreamingService() Failed!\n" );
        return false;
    }

    return true;	
}

bool TrackingOnly::UpdateImage()
{
    // get the frame header only
    vr::EVRTrackedCameraError nCameraError = m_pVRTrackedCamera->GetVideoStreamFrameBuffer( m_hTrackedCamera, vr::VRTrackedCameraFrameType_Undistorted, nullptr, 0, &m_frameHeader, sizeof( m_frameHeader ) );
    if ( nCameraError != vr::VRTrackedCameraError_None )
        return false;

    if ( m_frameHeader.nFrameSequence == m_nLastFrameSequence )
    {
        // frame hasn't changed yet, nothing to do
        return false;
    }

    // Frame has changed, do the more expensive frame buffer copy
    nCameraError = m_pVRTrackedCamera->GetVideoStreamFrameBuffer( m_hTrackedCamera, vr::VRTrackedCameraFrameType_Undistorted, m_pCameraFrameBuffer, m_nCameraFrameBufferSize, &m_frameHeader, sizeof( m_frameHeader ) );
    if ( nCameraError != vr::VRTrackedCameraError_None )
        return true;

    m_nLastFrameSequence = m_frameHeader.nFrameSequence;

    // this is needed ONLY for saving png
    if(false)
    {
            SDL_LockSurface(m_surface);
        memcpy(m_surface,m_pCameraFrameBuffer,m_nCameraFrameWidth*m_nCameraFrameHeight*4);
        SDL_UnlockSurface(m_surface);
    }
    SDL_UpdateTexture(m_tex,0,m_pCameraFrameBuffer,m_nCameraFrameWidth*4);
    return true;
}

void TrackingOnly::ReleaseVR()
{
    if ( m_pVRTrackedCamera )
    {
        m_pVRTrackedCamera->ReleaseVideoStreamingService( m_hTrackedCamera );
    }
}

void TrackingOnly::StopCapture()
{
    m_pVRTrackedCamera->ReleaseVideoStreamingService( m_hTrackedCamera );
    m_hTrackedCamera = INVALID_TRACKED_CAMERA_HANDLE;	

    SDL_FreeSurface(m_surface);
    SDL_DestroyTexture(m_tex);
}

bool TrackingOnly::InitOpenVR(SDL_Renderer *r)
{
	m_renderer = r;
    vr::EVRInitError eError = vr::VRInitError_None;
    m_pVRSystem = vr::VR_Init( &eError, vr::VRApplication_Background );
    if ( eError != vr::VRInitError_None )
    {
        m_pVRSystem = nullptr;
        char buf[1024];
        snprintf( buf, sizeof( buf ), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsSymbol( eError ) );
        LogMessage(  "%s\n", buf );
        return false;
    }
    else
    {
    	vr::HmdMatrix34_t mtx;
    	vr::ETrackedPropertyError errx;
        char systemName[1024];
        char serialNumber[1024];
        m_pVRSystem->GetStringTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, systemName, sizeof( systemName ) );
        m_pVRSystem->GetStringTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, serialNumber, sizeof( serialNumber ) );
        mtx = m_pVRSystem->GetMatrix34TrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_StatusDisplayTransform_Matrix34, &errx);

        m_cam2head = convert(mtx);

        m_HMDSerialNumberString = serialNumber;

        LogMessage(  "VR HMD: %s %s\n", systemName, serialNumber );
    }

    m_pVRTrackedCamera = vr::VRTrackedCamera();
    if ( !m_pVRTrackedCamera )
    {
        LogMessage(  "Unable to get Tracked Camera interface.\n" );
        return false;
    }
    LogMessage(  "m_pVRTrackedCamera OK\n" );

    bool bHasCamera = false;
    vr::EVRTrackedCameraError nCameraError = m_pVRTrackedCamera->HasCamera( vr::k_unTrackedDeviceIndex_Hmd, &bHasCamera );
    if ( nCameraError != vr::VRTrackedCameraError_None || !bHasCamera )
    {
        LogMessage(  "No Tracked Camera Available! (%s)\n", m_pVRTrackedCamera->GetCameraErrorNameFromEnum( nCameraError ) );
        return false;
    }
    LogMessage(  "bHasCamera OK\n" );

    // Accessing the FW description is just a further check to ensure camera communication is valid as expected.
    vr::ETrackedPropertyError propertyError;
    char buffer[128];
    m_pVRSystem->GetStringTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_CameraFirmwareDescription_String, buffer, sizeof( buffer ), &propertyError );
    if ( propertyError != vr::TrackedProp_Success )
    {
        LogMessage(  "Failed to get tracked camera firmware description!\n" );
        return false;
    }
    return true;
}

Uint32 my_callbackfunc(Uint32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    /* In this example, our callback pushes an SDL_USEREVENT event
    into the queue, and causes our callback to be called again at the
    same interval: */

    userevent.type = reinterpret_cast<intptr_t>(param);
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = reinterpret_cast<intptr_t>(param);
    event.user = userevent;

    SDL_PushEvent(&event);
    return(interval);
}

int main(int argc, char * argv[])
{
	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		printf("%s - SDL could not initialize! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	int WIDTH = 640;
	int HEIGHT = 480;
	auto w = SDL_CreateWindow( "tracked", 0,0,WIDTH,HEIGHT,SDL_WINDOW_SHOWN);
	SDL_SetWindowTitle( w, "tracked");
	auto renderer = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);
        if(!renderer)
             {
            LogMessage("Main Cannot Create Renderer\n");
            return -1;
        }

	// Prop_CameraToHeadTransform_Matrix34 for TrackedDeviceClass_HMD
	// Prop_StatusDisplayTransform_Matrix34 for ETrackedDeviceProperty


	SDL_StartTextInput();
	SDL_ShowCursor( SDL_DISABLE );
	SDL_Event sdlEvent;

	TrackingOnly vr;

	if(!vr.InitOpenVR(renderer))
		return false;
        fprintf(stderr,"InitOpenVR\n");
        if(!vr.StartCapture())
		return false;
        fprintf(stderr,"StartCapture\n");

	bool bQuit = false;
        SDL_TimerID my_timer_id = SDL_AddTimer((18 / 10) * 10, my_callbackfunc, (void*)SDL_USEREVENT);


	bool lastpressed = false;
        fprintf(stderr,"Ready! Go!\n");
        while(!bQuit)
	{
		// check timeout => update image
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, vr.m_tex, NULL, &vr.m_texr);
		SDL_RenderPresent(renderer);


		while ( SDL_PollEvent( &sdlEvent ) != 0 )
		{
			switch(sdlEvent.type)
			{
				case SDL_QUIT :
				{
					bQuit = true;
					break;
				}
				case SDL_KEYDOWN:
				{
					if ( sdlEvent.key.keysym.sym == SDLK_ESCAPE 
					     || sdlEvent.key.keysym.sym == SDLK_q )
					{
						break;
					}
					if( sdlEvent.key.keysym.sym == SDLK_c )
					{
					}
					break;
				}
				case SDL_USEREVENT:
				{
					vr.UpdateHMD();
                                        vr.UpdateImage();

					// and THEN use:
					// 		camera2head: vr.m_cam2head for 
					//		eye2head:    convert(vr.m_pVRSystem->GetEyeToHeadTransform( Eye_Left/Eye_Right ))
					//		head2origin: vr.m_hmdpose
					break;
				}
			}
		}

		// poll vr
		bool newpressed = vr.pollVRforButton();
		if(!newpressed && lastpressed)
		{
			printf("SAVE\n");
		}
		lastpressed = newpressed;
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(w);
	SDL_StopTextInput();
	return true;

}
