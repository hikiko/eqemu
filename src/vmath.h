/*
eqemu - electronic queue system emulator
Copyright (C) 2014  John Tsiombikas <nuclear@member.fsf.org>,
                    Eleni-Maria Stea <eleni@mutantstargoat.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef VMATH_H_
#define VMATH_H_

#include <math.h>

class Vector2 {
public:
	float x, y;

	Vector2() : x(0), y(0) {}
	Vector2(float xa, float ya) : x(xa), y(ya) {}

	float &operator [](int idx) { return (&x)[idx]; }
	const float &operator [](int idx) const { return (&x)[idx]; }
};

class Vector3 {
public:
	float x, y, z;

	Vector3() : x(0), y(0), z(0) {}
	Vector3(float xa, float ya, float za) : x(xa), y(ya), z(za) {}

	float &operator [](int idx) { return (&x)[idx]; }
	const float &operator [](int idx) const { return (&x)[idx]; }
};

inline Vector3 operator +(const Vector3 &a, const Vector3 &b)
{
	return Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline Vector3 operator -(const Vector3 &a, const Vector3 &b)
{
	return Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline Vector3 operator *(const Vector3 &a, float s)
{
	return Vector3(a.x * s, a.y * s, a.z * s);
}

inline float dot(const Vector3 &a, const Vector3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length(const Vector3 &v)
{
	return sqrt(dot(v, v));
}

inline Vector3 normalize(const Vector3 &v)
{
	float len = length(v);
	if(len == 0.0) {
		return v;
	}
	return Vector3(v.x / len, v.y / len, v.z / len);
}

class Vector4 {
public:
	float x, y, z, w;

	Vector4() : x(0), y(0), z(0), w(0) {}
	Vector4(float xa, float ya, float za, float wa) : x(xa), y(ya), z(za), w(wa) {}

	float &operator [](int idx) { return (&x)[idx]; }
	const float &operator [](int idx) const {  return (&x)[idx]; }
};

class Ray {
public:
	Vector3 origin, dir;

	Ray() : origin(0, 0, 0), dir(0, 0, 1) {}
	Ray(const Vector3 &o, const Vector3 &d) : origin(o), dir(d) {}
};

#endif	// VMATH_H_
