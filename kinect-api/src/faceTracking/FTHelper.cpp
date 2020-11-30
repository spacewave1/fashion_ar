﻿//------------------------------------------------------------------------------
// <copyright file="FTHelper.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include <iostream>
#include "FTHelper.h"
//#include "Visualize.h"

#ifdef SAMPLE_OPTIONS
#include "Options.h"
#else
PVOID _opt = NULL;
#endif

FTHelper::FTHelper()
{
    m_pFaceTracker = 0;
    m_pFTResult = NULL;
    m_colorImage = NULL;
    m_depthImage = NULL;
    m_ApplicationIsRunning = false;
    m_LastTrackSucceeded = false;
    m_CallBack = NULL;
    m_XCenterFace = 0;
    m_YCenterFace = 0;
    m_hFaceTrackingThread = NULL;
    m_DrawMask = TRUE;
    m_depthType = NUI_IMAGE_TYPE_DEPTH;
    m_depthRes = NUI_IMAGE_RESOLUTION_INVALID;
    m_bNearMode = FALSE;
    m_bFallbackToDefault = FALSE;
    m_colorType = NUI_IMAGE_TYPE_COLOR;
    m_colorRes = NUI_IMAGE_RESOLUTION_INVALID;
}

FTHelper::~FTHelper()
{
    Stop();
}

HRESULT FTHelper::Init(FTHelperCallBack callBack, PVOID callBackParam,
                       NUI_IMAGE_TYPE depthType, NUI_IMAGE_RESOLUTION depthRes, BOOL bNearMode, BOOL bFallbackToDefault, NUI_IMAGE_TYPE colorType, NUI_IMAGE_RESOLUTION colorRes, BOOL bSeatedSkeletonMode)
{
    if (!callBack)
    {
        return E_INVALIDARG;
    }
    m_CallBack = callBack;
    m_CallBackParam = callBackParam;
    m_ApplicationIsRunning = true;
    m_depthType = depthType;
    m_depthRes = depthRes;
    m_bNearMode = bNearMode;
    m_bFallbackToDefault = bFallbackToDefault;
    m_bSeatedSkeletonMode = bSeatedSkeletonMode;
    m_colorType = colorType;
    m_colorRes = colorRes;
    m_hFaceTrackingThread = CreateThread(NULL, 0, FaceTrackingStaticThread, (PVOID)this, 0, 0);
    return S_OK;
}

HRESULT FTHelper::Stop()
{
    m_ApplicationIsRunning = false;
    if (m_hFaceTrackingThread)
    {
        WaitForSingleObject(m_hFaceTrackingThread, 1000);
    }
    m_hFaceTrackingThread = 0;
    return S_OK;
}

BOOL FTHelper::SubmitFraceTrackingResult(IFTResult* pResult)
{
    if (pResult != NULL && SUCCEEDED(pResult->GetStatus()))
    {
        if (m_CallBack)
        {
            (*m_CallBack)(m_CallBackParam);
        }

        if (m_DrawMask)
        {
            FLOAT* pSU = NULL;
            UINT numSU;
            BOOL suConverged;
            m_pFaceTracker->GetShapeUnits(NULL, &pSU, &numSU, &suConverged);
            POINT viewOffset = {0, 0};
            FT_CAMERA_CONFIG cameraConfig;
            if (m_KinectSensorPresent)
            {
                m_KinectSensor.GetVideoConfiguration(&cameraConfig);
            }
            else
            {
                cameraConfig.Width = 640;
                cameraConfig.Height = 480;
                cameraConfig.FocalLength = 500.0f;
            }
            IFTModel* ftModel;
            HRESULT hr = m_pFaceTracker->GetFaceModel(&ftModel);
            if (SUCCEEDED(hr))
            {

                IFTModel* pModel;
                IFTResult* pAAMRlt = GetResult();
                FLOAT* pSUCoef;
                UINT coefCount;
                POINT viewOffset = {0, 0};
                float zoomFactor =  1.0;
                pAAMRlt->GetAUCoefficients(&pSUCoef,&coefCount);
                GetTracker()->GetFaceModel(&pModel);

                if (!pModel  || !pSUCoef || !pAAMRlt)
                {
                    std::cout << "Null pointers error" << std::endl;
                    return false;
                }

                HRESULT hr = S_OK;
                UINT vertexCount = pModel->GetVertexCount();
                FT_VECTOR2D* pPts2D = reinterpret_cast<FT_VECTOR2D*>(_malloca(sizeof(FT_VECTOR2D) * vertexCount));
                if (pPts2D)
                {
                    FLOAT *pAUs;
                    UINT auCount;
                    hr = pAAMRlt->GetAUCoefficients(&pAUs, &auCount);
                    if (SUCCEEDED(hr))
                    {
                        FLOAT scale, rotationXYZ[3], translationXYZ[3];
                        hr = pAAMRlt->Get3DPose(&scale, rotationXYZ, translationXYZ);
                        if (SUCCEEDED(hr))
                        {
                            hr = pModel->GetProjectedShape(&cameraConfig, zoomFactor, viewOffset, pSUCoef, pModel->GetSUCount(), pAUs, auCount,
                                                           scale, rotationXYZ, translationXYZ, pPts2D, vertexCount);
                            if (SUCCEEDED(hr))
                            {
                                POINT* p3DMdl   = reinterpret_cast<POINT*>(_malloca(sizeof(POINT) * vertexCount));
                                if (p3DMdl)
                                {
                                    std::cout << "{" << std::endl;
                                    for (UINT i = 0; i < vertexCount; ++i)
                                    {
                                        p3DMdl[i].x = LONG(pPts2D[i].x + 0.5f);
                                        p3DMdl[i].y = LONG(pPts2D[i].y + 0.5f);
                                        std::cout << "\"" << i << "\":" << "{ \"x\":" << p3DMdl[i].x << ",\"y\":" << p3DMdl[i].y << "}" << std::endl;
                                    }

                                    std::cout << "}" << std::endl;


                                    FT_TRIANGLE* pTriangles;
                                    UINT triangleCount;
                                    hr = pModel->GetTriangles(&pTriangles, &triangleCount);
                                    if (SUCCEEDED(hr))
                                    {
                                        struct EdgeHashTable
                                        {
                                            UINT32* pEdges;
                                            UINT edgesAlloc;

                                            void Insert(int a, int b)
                                            {
                                                UINT32 v = (min(a, b) << 16) | max(a, b);
                                                UINT32 index = (v + (v << 8)) * 49157, i;
                                                for (i = 0; i < edgesAlloc - 1 && pEdges[(index + i) & (edgesAlloc - 1)] && v != pEdges[(index + i) & (edgesAlloc - 1)]; ++i)
                                                {
                                                }
                                                pEdges[(index + i) & (edgesAlloc - 1)] = v;
                                            }
                                        } eht;

                                        eht.edgesAlloc = 1 << UINT(log(2.f * (1 + vertexCount + triangleCount)) / log(2.f));
                                        eht.pEdges = reinterpret_cast<UINT32*>(_malloca(sizeof(UINT32) * eht.edgesAlloc));
                                        if (eht.pEdges)
                                        {
                                            ZeroMemory(eht.pEdges, sizeof(UINT32) * eht.edgesAlloc);
                                            for (UINT i = 0; i < triangleCount; ++i)
                                            {
                                                eht.Insert(pTriangles[i].i, pTriangles[i].j);
                                                eht.Insert(pTriangles[i].j, pTriangles[i].k);
                                                eht.Insert(pTriangles[i].k, pTriangles[i].i);
                                            }
                                            for (UINT i = 0; i < eht.edgesAlloc; ++i)
                                            {
                                                if(eht.pEdges[i] != 0)
                                                {
                                                    //pColorImg->DrawLine(p3DMdl[eht.pEdges[i] >> 16], p3DMdl[eht.pEdges[i] & 0xFFFF], color, 1);
                                                }
                                            }
                                            _freea(eht.pEdges);
                                        }

                                        // Render the face rect in magenta
                                        RECT rectFace;
                                        hr = pAAMRlt->GetFaceRect(&rectFace);
                                    }

                                    _freea(p3DMdl);
                                }
                                else
                                {
                                    hr = E_OUTOFMEMORY;
                                }
                            }
                        }
                    }
                    _freea(pPts2D);
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }

                return false ;
                //hr = VisualizeFaceModel(m_colorImage, ftModel, &cameraConfig, pSU, 1.0, viewOffset, pResult, 0x00FFFF00);
                //hr = VisualizeFaceModel(m_colorImage, ftModel, &cameraConfig, pSU, 1.0, viewOffset, pResult, 0x00FFFF00);
                //ftModel->Release();
            }
        }
    }
    return TRUE;
}

// We compute here the nominal "center of attention" that is used when zooming the presented image.
void FTHelper::SetCenterOfImage(IFTResult* pResult)
{
    float centerX = ((float)m_colorImage->GetWidth())/2.0f;
    float centerY = ((float)m_colorImage->GetHeight())/2.0f;
    if (pResult)
    {
        if (SUCCEEDED(pResult->GetStatus()))
        {
            RECT faceRect;
            pResult->GetFaceRect(&faceRect);
            centerX = (faceRect.left+faceRect.right)/2.0f;
            centerY = (faceRect.top+faceRect.bottom)/2.0f;
        }
        m_XCenterFace += 0.02f*(centerX-m_XCenterFace);
        m_YCenterFace += 0.02f*(centerY-m_YCenterFace);
    }
    else
    {
        m_XCenterFace = centerX;
        m_YCenterFace = centerY;
    }
}

// Get a video image and process it.
void FTHelper::CheckCameraInput()
{
    HRESULT hrFT = E_FAIL;

    if (m_KinectSensorPresent && m_KinectSensor.GetVideoBuffer())
    {
        HRESULT hrCopy = m_KinectSensor.GetVideoBuffer()->CopyTo(m_colorImage, NULL, 0, 0);
        if (SUCCEEDED(hrCopy) && m_KinectSensor.GetDepthBuffer())
        {
            hrCopy = m_KinectSensor.GetDepthBuffer()->CopyTo(m_depthImage, NULL, 0, 0);
        }
        // Do face tracking
        if (SUCCEEDED(hrCopy))
        {
            FT_SENSOR_DATA sensorData(m_colorImage, m_depthImage, m_KinectSensor.GetZoomFactor(), m_KinectSensor.GetViewOffSet());

            FT_VECTOR3D* hint = NULL;
            if (SUCCEEDED(m_KinectSensor.GetClosestHint(m_hint3D)))
            {
                hint = m_hint3D;
            }
            if (m_LastTrackSucceeded)
            {
                hrFT = m_pFaceTracker->ContinueTracking(&sensorData, hint, m_pFTResult);
            }
            else
            {
                hrFT = m_pFaceTracker->StartTracking(&sensorData, NULL, hint, m_pFTResult);
            }
        }
    }

    m_LastTrackSucceeded = SUCCEEDED(hrFT) && SUCCEEDED(m_pFTResult->GetStatus());
    if (m_LastTrackSucceeded)
    {
        std::cout << "Found Face" << std::endl;
        SubmitFraceTrackingResult(m_pFTResult);
    }
    else
    {
        m_pFTResult->Reset();
    }
    SetCenterOfImage(m_pFTResult);
}

DWORD WINAPI FTHelper::FaceTrackingStaticThread(PVOID lpParam)
{
    FTHelper* context = static_cast<FTHelper*>(lpParam);
    if (context)
    {
        return context->FaceTrackingThread();
    }
    return 0;
}

DWORD WINAPI FTHelper::FaceTrackingThread()
{
    FT_CAMERA_CONFIG videoConfig;
    FT_CAMERA_CONFIG depthConfig;
    FT_CAMERA_CONFIG* pDepthConfig = NULL;

    // Try to get the Kinect camera to work
    HRESULT hr = m_KinectSensor.Init(m_depthType, m_depthRes, m_bNearMode, m_bFallbackToDefault, m_colorType, m_colorRes, m_bSeatedSkeletonMode);
    if (SUCCEEDED(hr))
    {
        m_KinectSensorPresent = TRUE;
        m_KinectSensor.GetVideoConfiguration(&videoConfig);
        m_KinectSensor.GetDepthConfiguration(&depthConfig);
        pDepthConfig = &depthConfig;
        m_hint3D[0] = m_hint3D[1] = FT_VECTOR3D(0, 0, 0);
    }
    else
    {
        m_KinectSensorPresent = FALSE;
        WCHAR errorText[MAX_PATH];
        ZeroMemory(errorText, sizeof(WCHAR) * MAX_PATH);
        std::cout << "Could not initialize the Kinect sensor" << std::endl;
        return 1;
    }

    // Try to start the face tracker.
    m_pFaceTracker = FTCreateFaceTracker(_opt);
    if (!m_pFaceTracker)
    {
        std::cout << "Could not create the face tracker." << std::endl;
        return 2;
    }

    hr = m_pFaceTracker->Initialize(&videoConfig, pDepthConfig, NULL, NULL); 
    if (FAILED(hr))
    {
        WCHAR path[512], buffer[1024];
        GetCurrentDirectoryW(ARRAYSIZE(path), path);
        std::cout << "Could not initialize face tracker (%s)." << std::endl;

        return 3;
    }

    hr = m_pFaceTracker->CreateFTResult(&m_pFTResult);
    if (FAILED(hr) || !m_pFTResult)
    {
        std::cout << "Could not initialize the face tracker result." << std::endl;
        return 4;
    }

    // Initialize the RGB image.
    m_colorImage = FTCreateImage();
    if (!m_colorImage || FAILED(hr = m_colorImage->Allocate(videoConfig.Width, videoConfig.Height, FTIMAGEFORMAT_UINT8_B8G8R8X8)))
    {
        return 5;
    }
    std::cout << "Has allocated color image memory" << std::endl;

    if (pDepthConfig)
    {
        m_depthImage = FTCreateImage();
        if (!m_depthImage || FAILED(hr = m_depthImage->Allocate(depthConfig.Width, depthConfig.Height, FTIMAGEFORMAT_UINT16_D13P3)))
        {
            return 6;
        }
    }
    std::cout << "Has allocated depth image memory" << std::endl;

    SetCenterOfImage(NULL);
    m_LastTrackSucceeded = false;

    while (m_ApplicationIsRunning)
    {
        CheckCameraInput();
        Sleep(16);
    }

    m_pFaceTracker->Release();
    m_pFaceTracker = NULL;

    if(m_colorImage)
    {
        m_colorImage->Release();
        m_colorImage = NULL;
    }

    if(m_depthImage) 
    {
        m_depthImage->Release();
        m_depthImage = NULL;
    }

    if(m_pFTResult)
    {
        m_pFTResult->Release();
        m_pFTResult = NULL;
    }
    m_KinectSensor.Release();
    return 0;
}

HRESULT FTHelper::GetCameraConfig(FT_CAMERA_CONFIG* cameraConfig)
{
    return m_KinectSensorPresent ? m_KinectSensor.GetVideoConfiguration(cameraConfig) : E_FAIL;
}