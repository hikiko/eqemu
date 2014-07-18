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
#ifndef MESH_H_
#define MESH_H_

#include "bvol.h"

enum {
	MESH_ATTR_VERTEX,
	MESH_ATTR_NORMAL,
	MESH_ATTR_TEXCOORD,

	NUM_MESH_ATTRIBS
};

class Mesh {
private:
	float *attr[NUM_MESH_ATTRIBS];
	int vcount;
	unsigned int vbo[NUM_MESH_ATTRIBS];
	int attr_size[NUM_MESH_ATTRIBS];

	mutable unsigned int buf_valid;	/* bitmask */
	void update_buffers() const;

	mutable BSphere bsph;
	mutable bool bsph_valid;
	void calc_bsph() const;

public:
	Mesh();
	~Mesh();

	float *set_attrib(int aidx, int count, int elemsz, float *data = 0);
	float *get_attrib(int aidx);
	const float *get_attrib(int aidx) const;

	void draw() const;

	BSphere &get_bounds();
	const BSphere &get_bounds() const;
};

#endif	// MESH_H_
