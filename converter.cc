#include "converter.hh"
#include <stdexcept>
#include <span>
#include <fstream>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/texture.h>
#include <iostream>

texture_converter::texture_converter(
		const std::filesystem::path &from,
		const std::filesystem::path &to) : _image(), from_path(from), to_path(to) {
	if (!std::filesystem::exists(from))
		throw std::runtime_error(from.string() + ": does not exist");
	if (std::filesystem::status(from).type()
				== std::filesystem::file_type::directory)
		throw std::runtime_error(from.string() + ": is a directory");
	_image.read(from_path.string());
}

texture_converter::texture_converter(
		const std::string &from,
		const std::string &to)
	: texture_converter(std::filesystem::path(from),
			std::filesystem::path(to)) { }

texture_converter::~texture_converter() { }

void texture_converter::convert() {
	_image.write(to_path.string());
}

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

std::string converter::texture_name(const std::string &path) {
	return std::filesystem::path(path).stem().string();
}

void converter::convert_compressed_texture(const aiTexture *texture) {
	const std::string name = texture_name(texture->mFilename.C_Str());
	_out
		<< TEX_DIRECTIVE
		<< SEPARATOR
		<< TEX_PREFIX
		<< name << std::endl;
	std::filesystem::path tex_path =
		std::filesystem::current_path() / scene_name / (name + TEX_EXT);
	texture_converter(texture->mFilename.C_Str(), tex_path.string()).convert();
}
