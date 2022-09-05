#include "converter.hh"
#include <stdexcept>
#include <span>
#include <fstream>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/texture.h>
#include <Magick++.h>

converter::converter(const std::string &file, std::ostream &out)
	: _file(file),
	_out(out),
	_importer(),
	_scene(_importer.ReadFile(_file.c_str(), aiProcess_Triangulate)) { 
}

void converter::convert() {
	write_global_textures();
}

void converter::write_global_textures() {
	const std::span textures(_scene->mTextures, _scene->mNumTextures);

	for (aiTexture *texture : textures) {
		convert_texture(texture);
	}
}

void converter::convert_texture(const aiTexture *texture) {
	if (texture->mHeight == 0) {
		convert_raw_texture(texture);
	} else {
		convert_compressed_texture(texture);
	}
}

void converter::convert_raw_texture(const aiTexture *texture) {
	(void) texture;
	throw std::runtime_error("conversion of embedded textures is currently not implemented");
}

void converter::convert_compressed_texture(const aiTexture *texture) {
	const std::string name = texture_name(texture);
	_out
		<< TEX_DIRECTIVE
		<< SEPARATOR
		<< TEX_PREFIX
		<< name << std::endl;
	std::ofstream out(scene_name + "/" + name + TEX_EXT);
	texture_converter(texture->mFilename.C_Str(), out).convert();
}
