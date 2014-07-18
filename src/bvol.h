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
#ifndef BVOL_H_
#define BVOL_H_

#include "vmath.h"

struct HitPoint {
	float t;
	Vector3 pos;
};

class BVolume {
public:
	virtual ~BVolume() {}

	virtual bool intersect(const Ray &ray, HitPoint *hit) const = 0;
};

class BSphere : public BVolume {
private:
	Vector3 center;
	float radius;

public:
	BSphere();
	explicit BSphere(const Vector3 &c, float rad = 1.0);

	void set_center(const Vector3 &center);
	const Vector3 &get_center() const;

	void set_radius(float rad);
	float get_radius() const;

	bool intersect(const Ray &ray, HitPoint *hit) const;
};

#endif	// BVOL_H_
