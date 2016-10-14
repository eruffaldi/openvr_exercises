# openvr_exercises
For the course IVE


# Minimal OpenVR Notes

## Matrices

vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix( nEye, m_fNearClip, m_fFarClip, vr::API_OpenGL);
vr::HmdMatrix43_t mat = m_pHMD->GetEyeToHeadTransform( nEye );

	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );
	