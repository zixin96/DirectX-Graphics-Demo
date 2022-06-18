#include <windows.h> // for XMVerifyCPUSupport
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <iostream>
using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

// Overload the  "<<" operators so that we can use cout to 
// output XMVECTOR and XMMATRIX objects.
ostream& XM_CALLCONV operator <<(ostream& os, FXMVECTOR v)
{
	XMFLOAT4 dest;
	XMStoreFloat4(&dest, v);

	os << "(" << dest.x << ", " << dest.y << ", " << dest.z << ", " << dest.w << ")";
	return os;
}

ostream& XM_CALLCONV operator <<(ostream& os, FXMMATRIX m)
{
	for (int i = 0; i < 4; ++i)
	{
		os << XMVectorGetX(m.r[i]) << "\t";
		os << XMVectorGetY(m.r[i]) << "\t";
		os << XMVectorGetZ(m.r[i]) << "\t";
		os << XMVectorGetW(m.r[i]);
		os << endl;
	}
	return os;
}

ostream& operator <<(ostream& os, const float arr[4][4])
{
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			os << arr[i][j] << " ";
		}
		os << "\n";
	}
	os << endl;
	return os;
}

// compute inverse of a matrix and its determinant
bool gluInvertMatrix(float* retDet, const float input[4][4], float output[4][4])
{
	float m[16];
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			m[4 * i + j] = input[i][j];
		}
	}

	float invOut[16];
	float inv[16], det;
	int   i;

	inv[0] = m[5] * m[10] * m[15] -
	         m[5] * m[11] * m[14] -
	         m[9] * m[6] * m[15] +
	         m[9] * m[7] * m[14] +
	         m[13] * m[6] * m[11] -
	         m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] +
	         m[4] * m[11] * m[14] +
	         m[8] * m[6] * m[15] -
	         m[8] * m[7] * m[14] -
	         m[12] * m[6] * m[11] +
	         m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] -
	         m[4] * m[11] * m[13] -
	         m[8] * m[5] * m[15] +
	         m[8] * m[7] * m[13] +
	         m[12] * m[5] * m[11] -
	         m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] +
	          m[4] * m[10] * m[13] +
	          m[8] * m[5] * m[14] -
	          m[8] * m[6] * m[13] -
	          m[12] * m[5] * m[10] +
	          m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] +
	         m[1] * m[11] * m[14] +
	         m[9] * m[2] * m[15] -
	         m[9] * m[3] * m[14] -
	         m[13] * m[2] * m[11] +
	         m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
	         m[0] * m[11] * m[14] -
	         m[8] * m[2] * m[15] +
	         m[8] * m[3] * m[14] +
	         m[12] * m[2] * m[11] -
	         m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] +
	         m[0] * m[11] * m[13] +
	         m[8] * m[1] * m[15] -
	         m[8] * m[3] * m[13] -
	         m[12] * m[1] * m[11] +
	         m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
	          m[0] * m[10] * m[13] -
	          m[8] * m[1] * m[14] +
	          m[8] * m[2] * m[13] +
	          m[12] * m[1] * m[10] -
	          m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] -
	         m[1] * m[7] * m[14] -
	         m[5] * m[2] * m[15] +
	         m[5] * m[3] * m[14] +
	         m[13] * m[2] * m[7] -
	         m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] +
	         m[0] * m[7] * m[14] +
	         m[4] * m[2] * m[15] -
	         m[4] * m[3] * m[14] -
	         m[12] * m[2] * m[7] +
	         m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] -
	          m[0] * m[7] * m[13] -
	          m[4] * m[1] * m[15] +
	          m[4] * m[3] * m[13] +
	          m[12] * m[1] * m[7] -
	          m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] +
	          m[0] * m[6] * m[13] +
	          m[4] * m[1] * m[14] -
	          m[4] * m[2] * m[13] -
	          m[12] * m[1] * m[6] +
	          m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] +
	         m[1] * m[7] * m[10] +
	         m[5] * m[2] * m[11] -
	         m[5] * m[3] * m[10] -
	         m[9] * m[2] * m[7] +
	         m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
	         m[0] * m[7] * m[10] -
	         m[4] * m[2] * m[11] +
	         m[4] * m[3] * m[10] +
	         m[8] * m[2] * m[7] -
	         m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] +
	          m[0] * m[7] * m[9] +
	          m[4] * m[1] * m[11] -
	          m[4] * m[3] * m[9] -
	          m[8] * m[1] * m[7] +
	          m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
	          m[0] * m[6] * m[9] -
	          m[4] * m[1] * m[10] +
	          m[4] * m[2] * m[9] +
	          m[8] * m[1] * m[6] -
	          m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	*retDet = det;

	if (det == 0)
		return false;

	det = 1.0 / det;


	for (i = 0; i < 16; i++)
	{
		invOut[i] = inv[i] * det;
	}

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			output[i][j] = invOut[4 * i + j];
		}
	}

	return true;
}

int main()
{
	// Check support for SSE2 (Pentium4, AMD K8, and above).
	if (!XMVerifyCPUSupport())
	{
		cout << "directx math not supported" << endl;
		return 0;
	}

	float zzxA[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 2.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 4.0f, 0.0f},
		{1.0f, 2.0f, 3.0f, 1.0f}
	};

	// compute transpose
	float zzxATranpose[4][4];
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			zzxATranpose[j][i] = zzxA[i][j];
		}
	}

	// compute determinant and inverse
	float zzxAInv[4][4];
	float zzxADet;
	gluInvertMatrix(&zzxADet, zzxA, zzxAInv);

	cout << "A Computed = " << endl << zzxA << endl;
	cout << "A(Transpose) Computed = " << endl << zzxATranpose << endl;
	cout << "A(Determinant) Computed= " << endl << zzxADet << endl;
	cout << "A(Inverse) Computed= " << endl << zzxAInv << endl;

	// correctness check:
	XMMATRIX A(1.0f, 0.0f, 0.0f, 0.0f,
	           0.0f, 2.0f, 0.0f, 0.0f,
	           0.0f, 0.0f, 4.0f, 0.0f,
	           1.0f, 2.0f, 3.0f, 1.0f);
	XMVECTOR ADeter;
	XMMATRIX AInv = XMMatrixInverse(&ADeter, A);

	cout << "A = " << endl << A << endl;
	cout << "A(Transpose) = " << endl << XMMatrixTranspose(A) << endl;
	cout << "A(Determinant) = " << endl << ADeter << endl;
	cout << "A(Inverse) = " << endl << AInv << endl;
	return 0;
}
