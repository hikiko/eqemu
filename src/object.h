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
#ifndef OBJECT_H_
#define OBJECT_H_

#include <string>
#include "mesh.h"
#include "material.h"

class Object {
private:
	std::string name;
	Mesh *mesh;

public:
	Material mtl;

	Object();

	void set_name(const char *name);
	const char *get_name() const;

	void set_mesh(Mesh *mesh);
	Mesh *get_mesh() const;

	void render() const;
};

#endif	// OBJECT_H_
