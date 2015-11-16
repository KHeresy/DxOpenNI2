// STL Header
#include <iostream>
#include <sstream>
#include <string>

// Direct3D Header
#include <d3d9.h>
#include <d3dx9.h>

// Kinect for Windows SDK Header
#include <Kinect.h>

// export functions
__declspec(dllexport) bool __stdcall OpenNIInit( HWND, bool, LPDIRECT3DDEVICE9, WCHAR*, CHAR* );
__declspec(dllexport) void __stdcall OpenNIClean();
__declspec(dllexport) void __stdcall OpenNIDrawDepthMap( bool );
__declspec(dllexport) void __stdcall OpenNIDepthTexture( IDirect3DTexture9** );
__declspec(dllexport) void __stdcall OpenNIGetSkeltonJointPosition( int, D3DXVECTOR3* );
__declspec(dllexport) void __stdcall OpenNIIsTracking( bool* );
__declspec(dllexport) void __stdcall OpenNIGetVersion( float* );

// include libraries
#pragma comment(lib, "Kinect20.lib")

// global variables
IKinectSensor*			g_pSensor			= nullptr;
IBodyIndexFrameReader*	g_pBodyIndexReader	= nullptr;
IBodyFrameReader*		g_pBodyReader		= nullptr;
IBody**					g_aBody				= nullptr;
INT32					g_iBodyCount		= 0;
int						g_iWidth			= 0;
int						g_iHeight			= 0;

bool				g_bDrawPixels		= FALSE;
bool				g_bDrawBackground	= FALSE;
bool				g_bTracking			= FALSE;

D3DXVECTOR3			g_vJoints[18]; // 0:center 1:neck 2:head 3:shoulderL 4:elbowL 5:wristL 6:shoulderR 7:elbowR 8:wristR 9:legL 10:kneeL 11 ancleL 12:legR 13:kneeR 14:ancleR 15:torso 16:handL 17:handR

const float			NOT_WORK_POS = -999.0f;
int					texWidth;
int					texHeight;
IDirect3DTexture9*	g_DepthImg = NULL;

// DllMain
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	switch( fdwReason )
	{
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		OpenNIClean();
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

// FUNCTION:getClosestPowerOfTwo()
UINT getClosestPowerOfTwo( UINT n )
{
	unsigned int m = 2;
	while(m < n)
		m<<=1;

	return m;
}

void SetPosition(D3DXVECTOR3& point, const Joint& rJoint)
{
	if (rJoint.TrackingState == TrackingState_Tracked)
	{
		point.x = rJoint.Position.X;
		point.y = rJoint.Position.Y;
		point.z = rJoint.Position.Z;
	}
	else
	{
		point.x = NOT_WORK_POS;
		point.y = NOT_WORK_POS;
		point.z = NOT_WORK_POS;
	}
}

// FUNCTION:printError()
void printError( HWND hWnd, const char *name )
{
	std::stringstream mStream;
	mStream << name << "failed";
	MessageBoxA( hWnd, mStream.str().c_str(), "error", MB_OK );
}

// EXPORT FUNCTION:Clean()
__declspec(dllexport) void __stdcall OpenNIClean()
{
	g_bTracking = false;

	if (g_DepthImg)
	{
		g_DepthImg->Release();
		g_DepthImg = nullptr;
	}

	if (g_aBody != nullptr)
	{
		delete[] g_aBody;
		g_aBody = nullptr;
	}

	if (g_pBodyReader != nullptr)
	{
		g_pBodyReader->Release();
		g_pBodyReader = nullptr;
	}

	if (g_pBodyIndexReader != nullptr)
	{
		g_pBodyIndexReader->Release();
		g_pBodyIndexReader = nullptr;
	}

	if (g_pSensor != nullptr)
	{
		g_pSensor->Close();
		g_pSensor->Release();
		g_pSensor = nullptr;
	}
}

// EXPORT FUNCTION:Init()
__declspec(dllexport) bool __stdcall OpenNIInit( HWND hWnd, bool EngFlag, LPDIRECT3DDEVICE9 lpDevice, WCHAR* f_path, CHAR* onifilename )
{
	////
	g_bTracking = false;

	SetCurrentDirectoryW(f_path);

	bool bInitialized = true;
	//. Get default Sensor and open
	if (GetDefaultKinectSensor(&g_pSensor) == S_OK && g_pSensor->Open() == S_OK)
	{
		#pragma region Body Index
		// Get body index frame source
		IBodyIndexFrameSource*	pBodyIndexSource = nullptr;
		if (g_pSensor->get_BodyIndexFrameSource(&pBodyIndexSource) == S_OK)
		{
			// Get frame description
			IFrameDescription* pFrameDescription = nullptr;
			if (pBodyIndexSource->get_FrameDescription(&pFrameDescription) == S_OK)
			{
				pFrameDescription->get_Width(&g_iWidth);
				pFrameDescription->get_Height(&g_iHeight);

				// Initial Direct 3D texture
				texWidth = getClosestPowerOfTwo(g_iWidth / 4);
				texHeight = getClosestPowerOfTwo(g_iHeight / 4);

				if (FAILED(lpDevice->CreateTexture(texWidth, texHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_DepthImg, NULL)))
				{
					printError(hWnd, "Cannot create depth texture");
					bInitialized = false;
				}
			}
			pFrameDescription->Release();
			pFrameDescription = nullptr;

			//  get frame reader
			if (pBodyIndexSource->OpenReader(&g_pBodyIndexReader) == S_OK)
			{
				// release frame source
				pBodyIndexSource->Release();
				pBodyIndexSource = nullptr;
			}
			else
			{
				printError(hWnd, "Can't get body index frame reader");
				bInitialized = false;
			}
		}
		else
		{
			printError(hWnd, "Can't get body index frame source");
			bInitialized = false;
		}
		#pragma endregion

		#pragma region Body
		// Get body frame source
		IBodyFrameSource* pBodySource = nullptr;
		if (g_pSensor->get_BodyFrameSource(&pBodySource) == S_OK)
		{
			// Get the number of body
			if (pBodySource->get_BodyCount(&g_iBodyCount) == S_OK)
			{
				// Initialize body array
				g_aBody = new IBody*[g_iBodyCount];
				for (int i = 0; i < g_iBodyCount; ++i)
					g_aBody[i] = nullptr;

				// get frame reader
				if (pBodySource->OpenReader(&g_pBodyReader) == S_OK)
				{
					pBodySource->Release();
					pBodySource = nullptr;
				}
				else
				{
					printError(hWnd, "Can't get body frame reader");
					bInitialized = false;
				}
			}
			else
			{
				printError(hWnd, "Can't get body count");
				bInitialized = false;
			}
		}
		else
		{
			printError(hWnd, "Can't get body frame source");
			bInitialized = false;
		}
		#pragma endregion
	}
	else
	{
		printError(hWnd, "Get Sensor failed");
		bInitialized = false;
	}

	if (!bInitialized)
		OpenNIClean();

	return bInitialized;
}

// EXPORT FUNCTION:DrawDepthMap()
__declspec(dllexport) void __stdcall OpenNIDrawDepthMap( bool waitflag )
{
	if (g_bDrawPixels)
	{
		// Get last frame
		IBodyIndexFrame* pFrame = nullptr;
		if (g_pBodyIndexReader->AcquireLatestFrame(&pFrame) == S_OK)
		{
			// Fill OpenCV image
			UINT uSize = 0;
			BYTE* pBuffer = nullptr;
			if (pFrame->AccessUnderlyingBuffer(&uSize, &pBuffer) == S_OK)
			{
				for (int y = 0; y < g_iHeight; ++y)
				{
					for (int x = 0; x < g_iWidth; ++x)
					{
						int uBodyIdx = pBuffer[x + y * g_iWidth];
						if (uBodyIdx < 6)
						{
						}
						else
						{
						}
					}
				}
			}

			// release frame
			pFrame->Release();

			// Lock texture
			D3DLOCKED_RECT LPdest;
			g_DepthImg->LockRect(0, &LPdest, NULL, 0);
			UCHAR *pDestImage = (UCHAR*)LPdest.pBits;

			// TODO: Update texture

			// unlock texture
			g_DepthImg->UnlockRect(0);
		}
	}

	// Get last frame
	IBodyFrame* pFrame = nullptr;
	if (g_pBodyReader->AcquireLatestFrame(&pFrame) == S_OK)
	{
		// get Body data
		if (pFrame->GetAndRefreshBodyData(g_iBodyCount, g_aBody) == S_OK)
		{
			// for each body
			for (int i = 0; i < g_iBodyCount; ++i)
			{
				IBody* pBody = g_aBody[i];

				// check if is tracked
				BOOLEAN bTracked = false;
				if ((pBody->get_IsTracked(&bTracked) == S_OK) && bTracked)
				{
					// get joint position
					Joint aJoints[JointType::JointType_Count];
					if (pBody->GetJoints(JointType::JointType_Count, aJoints) == S_OK)
					{
						SetPosition(g_vJoints[0], aJoints[JointType_SpineBase]);
						SetPosition(g_vJoints[1], aJoints[JointType_Neck]);
						SetPosition(g_vJoints[2], aJoints[JointType_Head]);

						SetPosition(g_vJoints[3], aJoints[JointType_ShoulderLeft]);
						SetPosition(g_vJoints[4], aJoints[JointType_ElbowLeft]);
						SetPosition(g_vJoints[5], aJoints[JointType_WristLeft]);

						SetPosition(g_vJoints[6], aJoints[JointType_ShoulderRight]);
						SetPosition(g_vJoints[7], aJoints[JointType_ElbowRight]);
						SetPosition(g_vJoints[8], aJoints[JointType_WristRight]);

						SetPosition(g_vJoints[9], aJoints[JointType_HipLeft]);
						SetPosition(g_vJoints[10], aJoints[JointType_KneeLeft]);
						SetPosition(g_vJoints[11], aJoints[JointType_FootLeft]);

						SetPosition(g_vJoints[12], aJoints[JointType_HipRight]);
						SetPosition(g_vJoints[13], aJoints[JointType_KneeRight]);
						SetPosition(g_vJoints[14], aJoints[JointType_FootRight]);

						SetPosition(g_vJoints[15], aJoints[JointType_SpineMid]);
						SetPosition(g_vJoints[16], aJoints[JointType_HandLeft]);
						SetPosition(g_vJoints[17], aJoints[JointType_HandRight]);

						// shifht to center
						for (int i = 1; i < 18; ++i)
						{
							if (g_vJoints[i].y != NOT_WORK_POS)
							{
								g_vJoints[i].x -= g_vJoints[0].x;
								g_vJoints[i].y -= g_vJoints[0].y;
								g_vJoints[i].z -= g_vJoints[0].z;
							}
						}

						g_vJoints[0].y = 0.0f;
						g_bTracking = true;
						break;
					}
				}
			}
		}
		// release frame
		pFrame->Release();
	}
}

// DepthTexture()
__declspec(dllexport) void __stdcall OpenNIDepthTexture(IDirect3DTexture9** lpTex)
{
	*lpTex = g_DepthImg;
}

// GetSkeltonJointPosition()
__declspec(dllexport) void __stdcall OpenNIGetSkeltonJointPosition(int num,D3DXVECTOR3* vec)
{
	*vec = g_vJoints[num];
}

// IsTracking()
__declspec(dllexport) void __stdcall OpenNIIsTracking(bool* lpb)
{
	if(g_bTracking)
		*lpb = true;
	else
		*lpb = false;
}

// GetVersion()
__declspec(dllexport) void __stdcall OpenNIGetVersion(float* ver)
{
	*ver = 1.30f;
}
