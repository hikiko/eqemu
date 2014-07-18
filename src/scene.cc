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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"

Scene::~Scene()
{
	for(size_t i=0; i<objects.size(); i++) {
		delete objects[i];
	}
	for(size_t i=0; i<meshes.size(); i++) {
		delete meshes[i];
	}
}

bool Scene::load(const char *fname)
{
	FILE *fp = fopen(fname, "rb");
	if(!fp) {
		fprintf(stderr, "failed to open scene file: %s\n", fname);
		return false;
	}

	bool res = load_obj(fp);
	fclose(fp);
	return res;
}

void Scene::add_object(Object *obj)
{
	objects.push_back(obj);
}

void Scene::add_mesh(Mesh *mesh)
{
	meshes.push_back(mesh);
}

int Scene::get_num_objects() const
{
	return (int)objects.size();
}

int Scene::get_num_meshes() const
{
	return (int)meshes.size();
}

Object *Scene::get_object(int idx) const
{
	return objects[idx];
}

Object *Scene::get_object(const char *name) const
{
	for(size_t i=0; i<objects.size(); i++) {
		if(strcmp(objects[i]->get_name(), name) == 0) {
			return objects[i];
		}
	}
	return 0;
}

bool Scene::remove_object(Object *obj)
{
	for(size_t i=0; i<objects.size(); i++) {
		if(objects[i] == obj) {
			objects.erase(objects.begin() + i);
			return true;
		}
	}
	return false;
}

Mesh *Scene::get_mesh(int idx) const
{
	return meshes[idx];
}

void Scene::update(long msec)
{
}

void Scene::render() const
{
	for(size_t i=0; i<objects.size(); i++) {
		objects[i]->render();
	}
}
