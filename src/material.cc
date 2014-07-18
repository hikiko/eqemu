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
#include <string.h>
#include <GL/glew.h>
#include <imago2.h>
#include "material.h"

static const char *find_path(const char *fname);

Material::Material()
	: diffuse(1, 1, 1), specular(0, 0, 0)
{
	shininess = 1.0;
	alpha = 1.0;

	for(int i=0; i<NUM_TEXTURES; i++) {
		tex[i] = 0;
		tex_scale[i].x = tex_scale[i].y = 1.0f;
	}
	sdr = 0;
}

void Material::setup() const
{
	float amb[] = {ambient.x, ambient.y, ambient.z, 1.0};
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);

	float col[] = {diffuse.x, diffuse.y, diffuse.z, alpha};
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);

	float spec[] = {specular.x, specular.y, specular.z, 1.0};
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);

	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess > 128 ? 128 : shininess);

	float emit[] = {emissive.x, emissive.y, emissive.z, 1.0};
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emit);

	int num_tex = 0;
	if(tex[TEX_DIFFUSE]) {
		glActiveTexture(GL_TEXTURE0 + num_tex++);

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glTranslatef(tex_offset[TEX_DIFFUSE].x, tex_offset[TEX_DIFFUSE].y, 0);
		glScalef(tex_scale[TEX_DIFFUSE].x, tex_scale[TEX_DIFFUSE].y, 1);

		glBindTexture(GL_TEXTURE_2D, tex[TEX_DIFFUSE]);
		glEnable(GL_TEXTURE_2D);

		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}

	if(tex[TEX_ENVMAP]) {
		glActiveTexture(GL_TEXTURE0 + num_tex++);

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glTranslatef(tex_offset[TEX_ENVMAP].x, tex_offset[TEX_ENVMAP].y, 0);
		glScalef(tex_scale[TEX_ENVMAP].x, tex_scale[TEX_ENVMAP].y, 1);

		glBindTexture(GL_TEXTURE_2D, tex[TEX_ENVMAP]);
		glEnable(GL_TEXTURE_2D);

		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
	}


	glMatrixMode(GL_TEXTURE);
	for(int i=num_tex; i<4; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glDisable(GL_TEXTURE_2D);
		glLoadIdentity();
	}

	glActiveTexture(GL_TEXTURE0);
	glMatrixMode(GL_MODELVIEW);
}

unsigned int load_texture(const char *fname)
{
	int xsz, ysz;
	void *pixels;
	unsigned int tex;

	const char *path = find_path(fname);

	if(!(pixels = img_load_pixels(path, &xsz, &ysz, IMG_FMT_RGBA32))) {
		fprintf(stderr, "failed to load texture: %s\n", fname);
		return 0;
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, xsz, ysz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	img_free_pixels(pixels);
	return tex;
}

static const char *find_path(const char *fname)
{
	const char *ptr = fname + strlen(fname) - 1;

	do {
		while(*ptr != '/' && ptr > fname - 1) {
			ptr--;
		}

		FILE *fp = fopen(ptr + 1, "rb");
		if(fp) {
			fclose(fp);
			return ptr + 1;
		}
		ptr -= 1;

	} while(ptr >= fname);

	return fname;
}
