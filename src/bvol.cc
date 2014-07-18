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
#include <math.h>
#include "bvol.h"

BSphere::BSphere()
{
	radius = 1.0f;
}

BSphere::BSphere(const Vector3 &c, float rad)
	: center(c)
{
	radius = rad;
}

void BSphere::set_center(const Vector3 &center)
{
	this->center = center;
}

const Vector3 &BSphere::get_center() const
{
	return center;
}

void BSphere::set_radius(float rad)
{
	radius = rad;
}

float BSphere::get_radius() const
{
	return radius;
}

bool BSphere::intersect(const Ray &ray, HitPoint *hit) const
{
	float a = dot(ray.dir, ray.dir);
	float b = 2.0 * dot(ray.dir, ray.origin - center);
	float c = dot(ray.origin, ray.origin) + dot(center, center) -
		2.0 * dot(ray.origin, center) - radius * radius;

	float disc = b * b - 4.0f * a * c;
	if(disc < 1e-6) {
		return false;
	}

	float sqrt_disc = sqrt(disc);
	float x1 = (-b + sqrt_disc) / (2.0f * a);
	float x2 = (-b - sqrt_disc) / (2.0f * a);

	if(x1 < 1e-6) x1 = x2;
	if(x2 < 1e-6) x2 = x1;

	float t = x1 < x2 ? x1 : x2;
	if(t < 1e-6) {
		return false;
	}

	hit->t = t;
	hit->pos = ray.origin + ray.dir * t;
	return true;
}
