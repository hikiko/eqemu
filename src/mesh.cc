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
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include "mesh.h"

#define ALL_VALID	0xffffffff

Mesh::Mesh()
{
	buf_valid = ALL_VALID;
	bsph_valid = false;

	for(int i=0; i<NUM_MESH_ATTRIBS; i++) {
		attr[i] = 0;
		attr_size[i] = 0;
		buf_valid &= ~(1 << i);
	}
	vcount = 0;
	glGenBuffers(NUM_MESH_ATTRIBS, vbo);
}

Mesh::~Mesh()
{
	for(int i=0; i<NUM_MESH_ATTRIBS; i++) {
		delete [] attr[i];
	}
	glDeleteBuffers(NUM_MESH_ATTRIBS, vbo);
}

float *Mesh::set_attrib(int aidx, int count, int elemsz, float *data)
{
	delete [] attr[aidx];
	attr[aidx] = new float[count * elemsz];
	memcpy(attr[aidx], data, count * elemsz * sizeof *data);
	vcount = count;
	attr_size[aidx] = elemsz;
	buf_valid &= ~(1 << aidx);

	if(aidx == MESH_ATTR_VERTEX) {
		bsph_valid = false;
	}

	return attr[aidx];
}

float *Mesh::get_attrib(int aidx)
{
	buf_valid &= ~(1 << aidx);
	if(aidx == MESH_ATTR_VERTEX) {
		bsph_valid = false;
	}
	return attr[aidx];
}

const float *Mesh::get_attrib(int aidx) const
{
	return attr[aidx];
}

void Mesh::draw() const
{
	if(!vcount) return;

	update_buffers();

	if(!vbo[MESH_ATTR_VERTEX]) {
		fprintf(stderr, "trying to render without a vertex buffer\n");
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo[MESH_ATTR_VERTEX]);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(attr_size[MESH_ATTR_VERTEX], GL_FLOAT, 0, 0);

	if(vbo[MESH_ATTR_NORMAL]) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo[MESH_ATTR_NORMAL]);
		glEnableClientState(GL_NORMAL_ARRAY);
		glNormalPointer(GL_FLOAT, 0, 0);
	}
	if(vbo[MESH_ATTR_TEXCOORD]) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo[MESH_ATTR_TEXCOORD]);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(attr_size[MESH_ATTR_TEXCOORD], GL_FLOAT, 0, 0);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_TRIANGLES, 0, vcount);

	glDisableClientState(GL_VERTEX_ARRAY);
	if(vbo[MESH_ATTR_NORMAL]) {
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	if(vbo[MESH_ATTR_TEXCOORD]) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
}

BSphere &Mesh::get_bounds()
{
	calc_bsph();
	return bsph;
}

const BSphere &Mesh::get_bounds() const
{
	calc_bsph();
	return bsph;
}

void Mesh::update_buffers() const
{
	if(buf_valid == ALL_VALID) {
		return;
	}

	for(int i=0; i<NUM_MESH_ATTRIBS; i++) {
		if((buf_valid & (1 << i)) == 0) {
			glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
			glBufferData(GL_ARRAY_BUFFER, vcount * attr_size[i] * sizeof(float),
					attr[i], GL_STATIC_DRAW);
			buf_valid |= 1 << i;
		}
	}
}

void Mesh::calc_bsph() const
{
	if(bsph_valid || !vcount) {
		return;
	}

	Vector3 center;

	float *vptr = attr[MESH_ATTR_VERTEX];
	for(int i=0; i<vcount; i++) {
		center = center + Vector3(vptr[0], vptr[1], vptr[2]);
		vptr += 3;
	}
	center = center * (1.0f / (float)vcount);

	float max_lensq = 0.0f;
	vptr = attr[MESH_ATTR_VERTEX];
	for(int i=0; i<vcount; i++) {
		Vector3 v = Vector3(vptr[0], vptr[1], vptr[2]) - center;
		float lensq = dot(v, v);
		if(lensq > max_lensq) {
			max_lensq = lensq;
		}
	}

	bsph.set_center(center);
	bsph.set_radius(sqrt(max_lensq));

	bsph_valid = true;
}
