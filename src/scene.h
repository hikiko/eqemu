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
#ifndef SCENE_H_
#define SCENE_H_

#include <stdio.h>
#include <vector>
#include "mesh.h"
#include "object.h"

class Scene {
private:
	std::vector<Object*> objects;
	std::vector<Mesh*> meshes;

	bool load_obj(FILE *fp);	// defined in objfile.cc

public:
	~Scene();

	bool load(const char *fname);

	void add_object(Object *obj);
	void add_mesh(Mesh *mesh);

	int get_num_objects() const;
	int get_num_meshes() const;

	Object *get_object(int idx) const;
	Mesh *get_mesh(int idx) const;

	Object *get_object(const char *name) const;
	bool remove_object(Object *obj);

	void update(long msec);
	void render() const;
};

#endif	// SCENE_H_
