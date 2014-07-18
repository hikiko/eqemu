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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include "scene.h"

using namespace std;

#define COMMANDS	\
	CMD(V),			\
	CMD(VN),		\
	CMD(VT),		\
	CMD(F),			\
	CMD(O),			\
	CMD(G),			\
	CMD(MTLLIB),	\
	CMD(USEMTL),	\
	CMD(NEWMTL),	\
	CMD(KE),		\
	CMD(KA),		\
	CMD(KD),		\
	CMD(KS),		\
	CMD(NS),		\
	CMD(NI),		\
	CMD(D),			\
	CMD(TR),		\
	CMD(MAP_KD),	\
	CMD(MAP_REFL),	\
	CMD(MAP_BUMP),	\
	CMD(MAP_KS),	\
	CMD(MAP_NS),	\
	CMD(MAP_D),		\
	CMD(REFL),		\
	CMD(BUMP)

#define CMD(x)	CMD_##x
enum {
	COMMANDS,
	CMD_UNK
};
#undef CMD

#define CMD(x)	#x
static const char *cmd_names[] = {
	COMMANDS,
	0
};
#undef CMD


struct ObjFace {
	int elem;
	int v[4], n[4], t[4];
};

struct ObjFile {
	string cur_obj, cur_mat;
	vector<Vector3> v, vn;
	vector<Vector2> vt;
	vector<ObjFace> f;
};

typedef Vector3 Color;

struct ObjMat {
	string name;		// newmtl <name>
	Color ambient, diffuse, specular;	// Ka, Kd, Ks
	Color emissive;		// Ke
	float shininess;	// Ns
	float ior;			// Ni
	float alpha;		// d, Tr

	string tex_dif, tex_spec, tex_shin, tex_alpha;	// map_Kd, map_Ks, map_Ns, map_d
	string tex_refl;	// refl -type sphere|cube file
	string tex_bump;	// bump

	ObjMat() { reset(); }

	void reset() {
		ambient = diffuse = Color(0.5, 0.5, 0.5);
		specular = Color(0.0, 0.0, 0.0);
		name = tex_dif = tex_spec = tex_shin = tex_alpha = tex_refl = tex_bump = "";
		shininess = 0;
		ior = alpha = 1;
	}
};

static bool read_materials(FILE *fp, vector<ObjMat> *vmtl);
static Object *cons_object(ObjFile *obj);

static int get_cmd(char *str);
static bool is_int(const char *str);
static bool is_float(const char *str);
static bool parse_vec(Vector3 *vec);
static bool parse_color(Color *col);
static bool parse_face(ObjFace *face);
static const char *parse_map();


static map<string, Material> matlib;


#define INVALID_IDX		INT_MIN

#define SEP		" \t\n\r\v"
#define BUF_SZ	512
bool Scene::load_obj(FILE *fp)
{
	static int seq;
	char cur_name[16];
	stringstream sstr;

	ObjFile obj;

	sprintf(cur_name, "default%02d.obj", seq++);
	obj.cur_obj = cur_name;

	int prev_cmd = 0, obj_added = 0;
	for(;;) {
		Vector3 vec;
		ObjFace face;

		char line[BUF_SZ];
		fgets(line, sizeof line, fp);
		if(feof(fp)) {
			break;
		}

		char *tok;
		if(!(tok = strtok(line, SEP))) {
			continue; // ignore empty lines
		}

		int cmd;
		if((cmd = get_cmd(tok)) == -1) {
			continue; // ignore unknown commands ...
		}

		switch(cmd) {
		case CMD_V:
			if(!parse_vec(&vec)) {
				continue;
			}
			obj.v.push_back(vec);
			break;

		case CMD_VN:
			if(!parse_vec(&vec)) {
				continue;
			}
			obj.vn.push_back(vec);
			break;

		case CMD_VT:
			if(!parse_vec(&vec)) {
				continue;
			}
			vec.y = 1.0 - vec.y;
			obj.vt.push_back(Vector2(vec.x, vec.y));
			break;

		case CMD_O:
		case CMD_G:
			if(prev_cmd == CMD_O || prev_cmd == CMD_G) {
				break;	// just in case we've got both of them in a row
			}
			/* if we have any previous data, group them up, add the object
			 * and continue with the new one...
			 */
			if(!obj.f.empty()) {
				Object *robj = cons_object(&obj);
				robj->mtl = matlib[obj.cur_mat];
				add_object(robj);
				obj_added++;

				obj.f.clear();	// clean the face list
			}
			if((tok = strtok(0, SEP))) {
				obj.cur_obj = tok;
			} else {
				sprintf(cur_name, "default%02d.obj", seq++);
				obj.cur_obj = cur_name;
			}
			break;

		case CMD_MTLLIB:
			if((tok = strtok(0, SEP))) {
				FILE *mfile;
				if(!(mfile = fopen(tok, "rb"))) {
					fprintf(stderr, "failed to open material library: %s\n", tok);
					continue;
				}

				// load all materials of the mtl file into a vector
				vector<ObjMat> vmtl;
				if(!read_materials(mfile, &vmtl)) {
					continue;
				}
				fclose(mfile);

				// and add them all to the scene
				for(size_t i=0; i<vmtl.size(); i++) {
					Material mat;
					mat.ambient = vmtl[i].ambient;
					mat.diffuse = vmtl[i].diffuse;
					mat.specular = vmtl[i].specular;
					mat.shininess = vmtl[i].shininess;
					mat.emissive = vmtl[i].emissive;
					mat.alpha = vmtl[i].alpha;

					if(vmtl[i].tex_dif.length()) {
						mat.tex[TEX_DIFFUSE] = load_texture(vmtl[i].tex_dif.c_str());
					}
					if(vmtl[i].tex_refl.length()) {
						mat.tex[TEX_ENVMAP] = load_texture(vmtl[i].tex_refl.c_str());
					}

					matlib[vmtl[i].name] = mat;
				}
			}
			break;

		case CMD_USEMTL:
			if((tok = strtok(0, SEP))) {
				obj.cur_mat = tok;
			} else {
				obj.cur_mat = "";
			}
			break;

		case CMD_F:
			if(!parse_face(&face)) {
				continue;
			}

			// convert negative indices to regular indices
			for(int i=0; i<4; i++) {
				if(face.v[i] < 0 && face.v[i] != INVALID_IDX) {
					face.v[i] = obj.v.size() + face.v[i];
				}
				if(face.n[i] < 0 && face.n[i] != INVALID_IDX) {
					face.n[i] = obj.vn.size() + face.n[i];
				}
				if(face.t[i] < 0 && face.t[i] != INVALID_IDX) {
					face.t[i] = obj.vt.size() + face.t[i];
				}
			}

			// break quads into triangles if needed
			obj.f.push_back(face);
			if(face.elem == 4) {
				face.v[1] = face.v[2];
				face.n[1] = face.n[2];
				face.t[1] = face.t[2];

				face.v[2] = face.v[3];
				face.n[2] = face.n[3];
				face.t[2] = face.t[3];

				obj.f.push_back(face);
			}
			break;

		default:
			break;	// ignore unknown commands
		}

		prev_cmd = cmd;
	}

	// reached end of file...
	if(!obj.f.empty()) {
		Object *robj = cons_object(&obj);
		robj->mtl = matlib[obj.cur_mat];
		add_object(robj);
		obj_added++;
	}

	return obj_added > 0;
}

static Object *cons_object(ObjFile *obj)
{
	Object *robj;
	Vector3 *varr, *narr;
	Vector2 *tarr;

	int nelem = obj->f.size() * 3;

	assert(sizeof(Vector3) == 3 * sizeof(float));
	assert(sizeof(Vector2) == 2 * sizeof(float));

	try {
		robj = new Object;
		varr = new Vector3[nelem];
		narr = new Vector3[nelem];
		tarr = new Vector2[nelem];
	}
	catch(...) {
		return 0;
	}
	if(obj->cur_obj.length() > 0) {
		robj->set_name(obj->cur_obj.c_str());
	}

	// need at least one of each element
	bool added_norm = false, added_tc = false;
	if(obj->vn.empty()) {
		obj->vn.push_back(Vector3(0, 0, 0));
		added_norm = true;
	}
	if(obj->vt.empty()) {
		obj->vt.push_back(Vector2(0, 0));
		added_tc = true;
	}

	for(size_t i=0; i<obj->f.size(); i++) {
		for(int j=0; j<3; j++) {
			int idx = i * 3 + j;
			ObjFace *f = &obj->f[i];

			varr[idx] = obj->v[f->v[j]];
			narr[idx] = obj->vn[f->n[j] < 0 ? 0 : f->n[j]];

			float t = obj->vt[f->t[j] < 0 ? 0 : f->t[j]].x;
			float s = obj->vt[f->t[j] < 0 ? 0 : f->t[j]].y;
			tarr[idx] = Vector2(t, s);
		}
	}

	if(added_norm) {
		obj->vn.pop_back();
	}
	if(added_tc) {
		obj->vt.pop_back();
	}

	Mesh *mesh = new Mesh;
	mesh->set_attrib(MESH_ATTR_VERTEX, nelem, 3, &varr->x);
	mesh->set_attrib(MESH_ATTR_NORMAL, nelem, 3, &narr->x);
	mesh->set_attrib(MESH_ATTR_TEXCOORD, nelem, 2, &tarr->x);
	robj->set_mesh(mesh);

	printf("loaded object %s: %d faces\n", obj->cur_obj.c_str(), nelem / 3);

	delete [] varr;
	delete [] narr;
	delete [] tarr;
	return robj;
}

static bool read_materials(FILE *fp, vector<ObjMat> *vmtl)
{
	ObjMat mat;

	for(;;) {
		char line[BUF_SZ];
		fgets(line, sizeof line, fp);
		if(feof(fp)) {
			break;
		}

		char *tok;
		if(!(tok = strtok(line, SEP))) {
			continue;
		}

		int cmd;
		if((cmd = get_cmd(tok)) == -1) {
			continue;
		}

		switch(cmd) {
		case CMD_NEWMTL:
			// add the previous material, and start a new one
			if(mat.name.length() > 0) {
				vmtl->push_back(mat);
				mat.reset();
			}
			if((tok = strtok(0, SEP))) {
				mat.name = tok;
			}
			break;

		case CMD_KE:
			parse_color(&mat.emissive);
			break;

		case CMD_KA:
			parse_color(&mat.ambient);
			break;

		case CMD_KD:
			parse_color(&mat.diffuse);
			break;

		case CMD_KS:
			parse_color(&mat.specular);
			break;

		case CMD_NS:
			if((tok = strtok(0, SEP)) && is_float(tok)) {
				mat.shininess = atof(tok);
			}
			break;

		case CMD_NI:
			if((tok = strtok(0, SEP)) && is_float(tok)) {
				mat.ior = atof(tok);
			}
			break;

		case CMD_D:
		case CMD_TR:
			{
				Color c;
				if(parse_color(&c)) {
					mat.alpha = cmd == CMD_D ? c.x : 1.0 - c.x;
				}
			}
			break;

		case CMD_MAP_KD:
			mat.tex_dif = parse_map();
			break;

		case CMD_MAP_REFL:
			mat.tex_refl = parse_map();
			break;

		default:
			break;
		}
	}

	if(mat.name.length() > 0) {
		vmtl->push_back(mat);
	}
	return true;
}

static int get_cmd(char *str)
{
	char *s = str;
	while((*s = toupper(*s))) s++;

	for(int i=0; cmd_names[i]; i++) {
		if(strcmp(str, cmd_names[i]) == 0) {
			return i;
		}
	}
	return CMD_UNK;
}

static bool is_int(const char *str)
{
	char *tmp;
	strtol(str, &tmp, 10);
	return tmp != str;
}

static bool is_float(const char *str)
{
	char *tmp;
	strtod(str, &tmp);
	return tmp != str;
}

static bool parse_vec(Vector3 *vec)
{
	for(int i=0; i<3; i++) {
		char *tok;

		if(!(tok = strtok(0, SEP)) || !is_float(tok)) {
			if(i < 2) {
				return false;
			}
			vec->z = 0.0;
		} else {
			(*vec)[i] = atof(tok);
		}
	}
	return true;
}

static bool parse_color(Color *col)
{
	for(int i=0; i<3; i++) {
		char *tok;

		if(!(tok = strtok(0, SEP)) || !is_float(tok)) {
			col->y = col->z = col->x;
			return i > 0 ? true : false;
		}
		(*col)[i] = atof(tok);
	}
	return true;
}

static bool parse_face(ObjFace *face)
{
	char *tok[] = {0, 0, 0, 0};
	face->elem = 0;

	for(int i=0; i<4; i++) {
		if((!(tok[i] = strtok(0, SEP)) || !is_int(tok[i]))) {
			if(i < 3) return false;	// less than 3 verts? not a polygon
		} else {
			face->elem++;
		}
	}

	for(int i=0; i<4; i++) {
		char *subtok = tok[i];

		if(!subtok || !*subtok || !is_int(subtok)) {
			if(i < 3) {
				return false;
			}
			face->v[i] = INVALID_IDX;
		} else {
			face->v[i] = atoi(subtok);
			if(face->v[i] > 0) face->v[i]--;	/* convert to 0-based */
		}

		while(subtok && *subtok && *subtok != '/') {
			subtok++;
		}
		if(subtok && *subtok && *++subtok && is_int(subtok)) {
			face->t[i] = atoi(subtok);
			if(face->t[i] > 0) face->t[i]--;	/* convert to 0-based */
		} else {
			face->t[i] = INVALID_IDX;
		}

		while(subtok && *subtok && *subtok != '/') {
			subtok++;
		}
		if(subtok && *subtok && *++subtok && is_int(subtok)) {
			face->n[i] = atoi(subtok);
			if(face->n[i] > 0) face->n[i]--;	/* convert to 0-based */
		} else {
			face->n[i] = INVALID_IDX;
		}
	}

	return true;
}

static const char *parse_map()
{
	char *tok, *prev = 0;

	while((tok = strtok(0, SEP))) {
		prev = tok;
	}

	return prev ? prev : "";
}
