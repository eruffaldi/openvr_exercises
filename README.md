# openvr_exercises
For the course IVE


# Minimal OpenVR Notes

## Matrices

	vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix( nEye, m_fNearClip, m_fFarClip, vr::API_OpenGL);
	vr::HmdMatrix43_t mat = m_pHMD->GetEyeToHeadTransform( nEye );

	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

##Events
	vr::VREvent_t event;
	while( m_pHMD->PollNextEvent( &event, sizeof( event ) ) )
	{
		ProcessVREvent( event );
	}

##Controllers
	for( vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++ )
	{
		vr::VRControllerState_t state;
		if( m_pHMD->GetControllerState( unDevice, &state ) )
		{
			m_rbShowTrackedDevice[ unDevice ] = state.ulButtonPressed == 0;
		}
	}

##Composition

	vr::Texture_t leftEyeTexture = {(void*)leftEyeDesc.m_nResolveTextureId, vr::API_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture );
	vr::Texture_t rightEyeTexture = {(void*)rightEyeDesc.m_nResolveTextureId, vr::API_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture );

##Distorsion
