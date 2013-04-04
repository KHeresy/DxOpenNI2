// STL Header
#include <iostream>
#include <sstream>
#include <string>

// Direct3D Header
#include <d3d9.h>
#include <d3dx9.h>

// OpenNI Header
#include <NiTE.h>

// export functions
__declspec(dllexport) bool __stdcall OpenNIInit( HWND, bool, LPDIRECT3DDEVICE9, WCHAR*, CHAR* );
__declspec(dllexport) void __stdcall OpenNIClean();
__declspec(dllexport) void __stdcall OpenNIDrawDepthMap( bool );
__declspec(dllexport) void __stdcall OpenNIDepthTexture( IDirect3DTexture9** );
__declspec(dllexport) void __stdcall OpenNIGetSkeltonJointPosition( int, D3DXVECTOR3* );
__declspec(dllexport) void __stdcall OpenNIIsTracking( bool* );
__declspec(dllexport) void __stdcall OpenNIGetVersion( float* );

// include libraries
#pragma comment(lib, "OpenNI2.lib")
#pragma comment(lib, "NiTE2.lib")

// defines
#define MAX_DEPTH	10000
#define NCOLORS		10

// global variables
nite::UserTracker	g_UserTracker;
nite::Point3f		g_BP_Zero;

bool				g_bDrawPixels		= TRUE;
bool				g_bDrawBackground	= FALSE;
bool				TrackingF			= FALSE;

float				Colors[][3] ={{0,1,1},{0,0,1},{0,1,0},{1,1,0},{1,0,0},{1,.5,0},{.5,1,0},{0,.5,1},{.5,0,1},{1,1,.5},{1,1,1}};

int					texWidth;
int					texHeight;
int					TrCount[15];
float				g_pDepthHist[MAX_DEPTH];

D3DXVECTOR3			BP_Vector[18]; // 0:center 1:neck 2:head 3:shoulderL 4:elbowL 5:wristL 6:shoulderR 7:elbowR 8:wristR 9:legL 10:kneeL 11 ancleL 12:legR 13:kneeR 14:ancleR 15:torso 16:handL 17:handR
IDirect3DTexture9*	DepthTex = NULL;

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

// FUNCTION:PosCalc()
void PosCalc( const nite::Skeleton& rSkeleton, nite::JointType eJoint, D3DXVECTOR3* point )
{
	const nite::SkeletonJoint& rJoint = rSkeleton.getJoint( eJoint );
	if( rJoint.getPositionConfidence() < 0.5f )
	{
		const nite::Point3f& rPos = rJoint.getPosition();
		point->x = ( rPos.x - g_BP_Zero.x );
		point->y = ( rPos.y - g_BP_Zero.y );
		point->z = ( rPos.z - g_BP_Zero.z );
	}
	else
	{
		point->y = -999.0f;
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
	if( DepthTex )
	{
		DepthTex->Release();
		DepthTex = NULL;
	}
	g_UserTracker.destroy();
	nite::NiTE::shutdown();
	TrackingF = false;
}

// EXPORT FUNCTION:Init()
__declspec(dllexport) bool __stdcall OpenNIInit( HWND hWnd, bool EngFlag, LPDIRECT3DDEVICE9 lpDevice, WCHAR* f_path, CHAR* onifilename )
{
	TrackingF=false;
	for( int i = 0; i < 15; ++ i )
		TrCount[i] = 0;

	SetCurrentDirectoryW( f_path );

	if( nite::NiTE::initialize() == nite::STATUS_OK )
	{
		if( g_UserTracker.create() == nite::STATUS_OK )
		{
			nite::UserTrackerFrameRef mUserFrame;
			if( g_UserTracker.readFrame( &mUserFrame ) == nite::STATUS_OK )
			{
				openni::VideoFrameRef mDepthMap = mUserFrame.getDepthFrame();
				int x = mDepthMap.getWidth(),
					y = mDepthMap.getHeight();
				
				texWidth =  getClosestPowerOfTwo( x / 4 );
				texHeight = getClosestPowerOfTwo( y / 4 );
				
				if( FAILED( lpDevice->CreateTexture( texWidth, texHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &DepthTex, NULL ) ) )
				{
					MessageBox( hWnd, L"Cannot create depth texture", L"NiTE2", MB_OK );
					OpenNIClean();
					return false;
				}

				return true;
			}
			else
			{
				printError( hWnd, "UserTracker.readFrame" );
				MessageBox( hWnd, L"Cannot read user tracker frame", L"NiTE2", MB_OK );
				OpenNIClean();
				return false;
			}
		}
		else
		{
			printError( hWnd, "UserTracker.create" );
			MessageBox( hWnd, L"Cannot create user tracker", L"NiTE2", MB_OK );
			OpenNIClean();
			return false;
		}
	}
	else
	{
		printError( hWnd, "Init" );
		MessageBox( hWnd, L"Cannot initial NiTE", L"NiTE2", MB_OK );
		return false;
	}
}

// EXPORT FUNCTION:DrawDepthMap()
__declspec(dllexport) void __stdcall OpenNIDrawDepthMap( bool waitflag )
{
	nite::UserTrackerFrameRef mUserFrame;
	if( g_UserTracker.readFrame( &mUserFrame ) == nite::STATUS_OK )
	{
		const nite::UserMap& rUserMap = mUserFrame.getUserMap();
		const nite::UserId* pLabels = rUserMap.getPixels();

		openni::VideoFrameRef mDepthMap = mUserFrame.getDepthFrame();
		const openni::DepthPixel* pDepth = static_cast<const openni::DepthPixel*>( mDepthMap.getData() );
		
		int iXRes = mDepthMap.getWidth(),
			iYRes = mDepthMap.getHeight();
		
		D3DLOCKED_RECT LPdest;
		DepthTex->LockRect(0,&LPdest,NULL, 0);
		UCHAR *pDestImage=(UCHAR*)LPdest.pBits;
		
		// Calculate the accumulative histogram
		ZeroMemory( g_pDepthHist, MAX_DEPTH * sizeof(float) );
		UINT nValue=0;
		UINT nNumberOfPoints = 0;
		for( int nY = 0; nY < iYRes; ++ nY )
		{
			for( int nX = 0; nX < iXRes; ++ nX )
			{
				nValue = *pDepth;
				if(nValue !=0)
				{
					g_pDepthHist[nValue]++;
					nNumberOfPoints++;
				}
				pDepth++;
			}
		}
		
		for( int nIndex = 1; nIndex < MAX_DEPTH; nIndex++ )
		{
			g_pDepthHist[nIndex] += g_pDepthHist[nIndex-1];
		}
		
		if( nNumberOfPoints )
		{
			for( int nIndex = 1; nIndex < MAX_DEPTH; nIndex++ )
			{
				g_pDepthHist[nIndex] = (float)((UINT)(256 * (1.0f - (g_pDepthHist[nIndex] / nNumberOfPoints))));
			}
		}
		
		UINT nHistValue = 0;
		if( g_bDrawPixels )
		{
			// Prepare the texture map
			for( int nY = 0; nY < iYRes; nY += 4 )
			{
				for( int nX=0; nX < iXRes; nX += 4 )
				{
					pDestImage[0] = 0;
					pDestImage[1] = 0;
					pDestImage[2] = 0;
					pDestImage[3] = 0;
					
					if( g_bDrawBackground )
					{
						nValue = *pDepth;
						nite::UserId label = *pLabels;

						int nColorID = label % NCOLORS;
						if( label == 0 )
							nColorID = NCOLORS;
						
						if(nValue != 0)
						{
							nHistValue = (UINT)(g_pDepthHist[nValue]);

							pDestImage[0] = (UINT)(nHistValue * Colors[nColorID][0]); 
							pDestImage[1] = (UINT)(nHistValue * Colors[nColorID][1]);
							pDestImage[2] = (UINT)(nHistValue * Colors[nColorID][2]);
							pDestImage[3] = 255;
						}
					}

					pDepth		+= 4;
					pLabels		+= 4;
					pDestImage	+= 4;
				}
				
				int pg = iXRes * 3;
				pDepth += pg;
				pLabels += pg;
				pDestImage += (texWidth - iXRes)*4+pg;
			}
		}
		else
		{
			memset( LPdest.pBits, 0, 4 * 2 * iXRes * iYRes );
		}
		DepthTex->UnlockRect(0);

		const nite::Array<nite::UserData>& aUsers = mUserFrame.getUsers();
		for( int iIdx = 0; iIdx < aUsers.getSize(); ++ iIdx )
		{
			const nite::UserData& rUser = aUsers[iIdx];
			if( rUser.isNew() )
			{
				g_UserTracker.startPoseDetection( rUser.getId(), nite::POSE_PSI );
			}
			else
			{
				const nite::PoseData& rPose = rUser.getPose( nite::POSE_PSI );
				if( rPose.isEntered() )
				{
					g_UserTracker.stopPoseDetection( rUser.getId(), nite::POSE_PSI );
					g_UserTracker.startSkeletonTracking( rUser.getId() );
				}

				const nite::Skeleton& rSkeleton = rUser.getSkeleton();
				if( rSkeleton.getState() == nite::SKELETON_TRACKED )
				{
					if( TrCount[iIdx] < 4 )
					{
						TrCount[iIdx]++;
						if( TrCount[iIdx] == 4 )
						{
							TrackingF = true;
							const nite::Point3f& rPos = rSkeleton.getJoint( nite::JOINT_TORSO ).getPosition();
							g_BP_Zero.x = rPos.x;
							g_BP_Zero.z = rPos.z;
							g_BP_Zero.y = float( rSkeleton.getJoint( nite::JOINT_LEFT_HIP ).getPosition().y + rSkeleton.getJoint( nite::JOINT_RIGHT_HIP ).getPosition().y ) / 2;
						}
					}

					PosCalc( rSkeleton, nite::JOINT_TORSO,			&BP_Vector[0] );
					PosCalc( rSkeleton, nite::JOINT_NECK,			&BP_Vector[1]);
					PosCalc( rSkeleton, nite::JOINT_HEAD,			&BP_Vector[2]);
					PosCalc( rSkeleton, nite::JOINT_LEFT_SHOULDER,	&BP_Vector[3]);
					PosCalc( rSkeleton, nite::JOINT_LEFT_ELBOW,		&BP_Vector[4]);
					PosCalc( rSkeleton, nite::JOINT_RIGHT_SHOULDER,	&BP_Vector[6]);
					PosCalc( rSkeleton, nite::JOINT_RIGHT_ELBOW,	&BP_Vector[7]);
					PosCalc( rSkeleton, nite::JOINT_LEFT_HIP,		&BP_Vector[9]);
					PosCalc( rSkeleton, nite::JOINT_LEFT_KNEE,		&BP_Vector[10]);
					PosCalc( rSkeleton, nite::JOINT_LEFT_FOOT,		&BP_Vector[11]);
					PosCalc( rSkeleton, nite::JOINT_RIGHT_HIP,		&BP_Vector[12]);
					PosCalc( rSkeleton, nite::JOINT_RIGHT_KNEE,		&BP_Vector[13]);
					PosCalc( rSkeleton, nite::JOINT_RIGHT_FOOT,		&BP_Vector[14]);
					PosCalc( rSkeleton, nite::JOINT_TORSO,			&BP_Vector[15]);
					PosCalc( rSkeleton, nite::JOINT_LEFT_HAND,		&BP_Vector[16]);
					PosCalc( rSkeleton, nite::JOINT_RIGHT_HAND,		&BP_Vector[17]);
					//PosCalc( rSkeleton, nite::XN_SKEL_LEFT_WRIST,	&BP_Vector[5]);
					//PosCalc( rSkeleton, nite::XN_SKEL_RIGHT_WRIST,	&BP_Vector[8]);

					BP_Vector[5] = BP_Vector[16];
					BP_Vector[8] = BP_Vector[17];

					BP_Vector[0].y = ( BP_Vector[9].y + BP_Vector[12].y ) / 2.0f;
					break;
				}
				else
				{
					TrCount[iIdx]=0;
				}
			}
		}
	}
}

// DepthTexture()
__declspec(dllexport) void __stdcall OpenNIDepthTexture(IDirect3DTexture9** lpTex)
{
	*lpTex = DepthTex;
}

// GetSkeltonJointPosition()
__declspec(dllexport) void __stdcall OpenNIGetSkeltonJointPosition(int num,D3DXVECTOR3* vec)
{
	*vec = BP_Vector[num];
}

// IsTracking()
__declspec(dllexport) void __stdcall OpenNIIsTracking(bool* lpb)
{
	if(TrackingF)
		*lpb = true;
	else
		*lpb = false;
}

// GetVersion()
__declspec(dllexport) void __stdcall OpenNIGetVersion(float* ver)
{
	*ver = 1.30f;
}
