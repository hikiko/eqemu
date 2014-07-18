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
#ifndef MATERIAL_H_
#define MATERIAL_H_

#include "vmath.h"

enum {
	TEX_DIFFUSE,
	TEX_ENVMAP,

	NUM_TEXTURES
};

class Material {
public:
	Vector3 emissive;
	Vector3 ambient;
	Vector3 diffuse;
	Vector3 specular;
	float shininess;
	float alpha;

	unsigned int tex[NUM_TEXTURES];
	Vector2 tex_scale[NUM_TEXTURES], tex_offset[NUM_TEXTURES];

	unsigned int sdr;

	Material();

	void setup() const;
};

unsigned int load_texture(const char *fname);
unsigned int load_shader_program(const char *vname, const char *pname);

#endif	// MATERIAL_H_
