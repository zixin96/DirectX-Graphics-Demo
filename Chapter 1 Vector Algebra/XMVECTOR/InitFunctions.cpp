// #include <Windows.h> // for XMVerifyCPUSupport
// #include <DirectXMath.h>
// #include <DirectXPackedVector.h>
// #include <iostream>
// using namespace std;
// using namespace DirectX;
// using namespace DirectX::PackedVector;
//
// // Overload the  "<<" operators so that we can use cout to 
// // output XMVECTOR objects.
// ostream& XM_CALLCONV operator<<(ostream& os, FXMVECTOR v)
// {
// 	XMFLOAT4 dest;
// 	XMStoreFloat4(&dest, v);
// 	//! We access the components of FXMVECTOR by first storing them into a XMFLOAT4, and use it to access x,y,z,w
// 	os << "(" << dest.x << ", " << dest.y << ", " << dest.z << ", " << dest.w << ")";
// 	return os;
// }
//
// int main()
// {
// 	//! Sets the cout formatting flag to use bool type in alphanumeric format
// 	cout.setf(ios_base::boolalpha);
//
// 	// Check support for SSE2 (Pentium4, AMD K8, and above).
// 	if (!XMVerifyCPUSupport())
// 	{
// 		cout << "directx math not supported" << endl;
// 		return 0;
// 	}
//
// 	XMVECTOR p = XMVectorZero();
// 	XMVECTOR q = XMVectorSplatOne();
// 	XMVECTOR u = XMVectorSet(1.0f, 2.0f, 3.0f, 999.0f);
// 	XMVECTOR v = XMVectorReplicate(-2.0f);
// 	XMVECTOR w = XMVectorSplatZ(u);
//
// 	cout << "p = " << p << endl;
// 	cout << "q = " << q << endl;
// 	cout << "u = " << u << endl;
// 	cout << "v = " << v << endl;
// 	cout << "w = " << w << endl;
//
// 	return 0;
// }
